#include "inferenceopv.h"
#include <iostream>
#include <string>
#include <time.h>

InferenceOPV::InferenceOPV(std::string model_path, const int height, const int width, const float proThreshold, const float nmsThreshold) : m_height(height), m_width(width), m_proThreshold(proThreshold), m_nmsThreshold(nmsThreshold)
{
	ov::Core core;
	std::shared_ptr<ov::Model> model = core.read_model(model_path);
	ov::preprocess::PrePostProcessor ppp = ov::preprocess::PrePostProcessor(model);
	ppp.input().tensor().set_element_type(ov::element::u8).set_layout("NHWC").set_color_format(ov::preprocess::ColorFormat::RGB);
	ppp.input().preprocess().convert_element_type(ov::element::f32).convert_color(ov::preprocess::ColorFormat::RGB).scale({ 255, 255, 255 });// .scale({ 112, 112, 112 });
	ppp.input().model().set_layout("NCHW");
	ppp.output().tensor().set_element_type(ov::element::f32);
	model = ppp.build();
	this->compiled_model = core.compile_model(model, "CPU");
	this->infer_request = compiled_model.create_infer_request();
}

std::vector<Object> InferenceOPV::detect(const cv::Mat& inputImgBGR) {
	preprocess(inputImgBGR);
	infer_request.infer();
	const ov::Tensor& output_tensor = infer_request.get_output_tensor();
	ov::Shape output_shape = output_tensor.get_shape();
	float* detections = output_tensor.data<float>();
	std::cout << "[DEBUG] Start detection image" << std::endl;
	return this->postprocess(inputImgBGR, detections, output_shape);
}

void InferenceOPV::preprocess(const cv::Mat& frame) {
	try {
		float width = frame.cols;
		float height = frame.rows;
		cv::Size new_shape = cv::Size(m_width, m_height);
		float r = float(new_shape.width / cv::max(width, height));
		int new_unpadW = int(round(width * r));
		int new_unpadH = int(round(height * r));

		cv::resize(frame, resize.resized_image, cv::Size(new_unpadW, new_unpadH), 0, 0, cv::INTER_AREA);
		resize.resized_image = resize.resized_image;
		resize.dw = new_shape.width - new_unpadW;
		resize.dh = new_shape.height - new_unpadH;
		cv::Scalar color = cv::Scalar(100, 100, 100);
		cv::copyMakeBorder(resize.resized_image, resize.resized_image, 0, resize.dh, 0, resize.dw, cv::BORDER_CONSTANT, color);

		this->rx = (float)frame.cols / (float)(resize.resized_image.cols - resize.dw);
		this->ry = (float)frame.rows / (float)(resize.resized_image.rows - resize.dh);
		float* input_data = (float*)resize.resized_image.data;
		input_tensor = ov::Tensor(compiled_model.input().get_element_type(), compiled_model.input().get_shape(), input_data);
		infer_request.set_input_tensor(input_tensor);
	}
	catch (const std::exception& e) {
		std::cerr << "exception: " << e.what() << std::endl;
	}
	catch (...) {
		std::cerr << "unknown exception" << std::endl;
	}

}

std::vector <Object> InferenceOPV::postprocess(const cv::Mat& frame, float* detections, ov::Shape& output_shape) {
	std::vector<cv::Rect> boxes;
	std::vector<int> class_ids;
	std::vector<float> confidences;
	int out_rows = output_shape[1];
	int out_cols = output_shape[2];
	const cv::Mat det_output(out_rows, out_cols, CV_32F, (float*)detections);

	for (int i = 0; i < det_output.cols; ++i) {
		const cv::Mat classes_scores = det_output.col(i).rowRange(4, 84);
		cv::Point class_id_point;
		double score;
		cv::minMaxLoc(classes_scores, nullptr, &score, nullptr, &class_id_point);

		if (score > m_proThreshold) { // config.confThreshold or config.scoreThreshold?
			const float cx = det_output.at<float>(0, i);
			const float cy = det_output.at<float>(1, i);
			const float ow = det_output.at<float>(2, i);
			const float oh = det_output.at<float>(3, i);
			cv::Rect box;
			box.x = static_cast<int>((cx - 0.5 * ow));
			box.y = static_cast<int>((cy - 0.5 * oh));
			box.width = static_cast<int>(ow);
			box.height = static_cast<int>(oh);

			boxes.push_back(box);
			class_ids.push_back(class_id_point.y);
			confidences.push_back(score);
		}
	}

	std::vector<int> nms_result;
	cv::dnn::NMSBoxes(boxes, confidences, m_proThreshold, m_nmsThreshold, nms_result);

	std::vector<Object> output;
	for (int i = 0; i < nms_result.size(); i++)
	{
		Object result;
		int idx = nms_result[i];
		result.class_id = class_ids[idx];
		result.confidence = confidences[idx];
		result.box = boxes[idx];
		output.push_back(result);
	}

	return output;
}