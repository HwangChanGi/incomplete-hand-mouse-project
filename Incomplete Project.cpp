#include <opencv2/opencv.hpp>		// opencv �������
#include <iostream>			// �����������
using namespace cv;			// cv����
using namespace std;			// std����
using namespace cv::dnn;		// dnn ����

const float CONFIDENCE_THRESHOLD = 0.5;	// �˰��� Ȯ�Ű�
const float NMS_THRESHOLD = 0.5;			// �� �ִ밪 ����
const int NUM_CLASSES = 2;			// Ŭ���� 2

const Scalar colors[] = {		// �� ��İ�
{0, 255, 255},
{255, 255, 0},
{0, 255, 0},
{255, 0, 0}
};
const auto NUM_COLORS = sizeof(colors) / sizeof(colors[0]);	// �� ������ ũ�� Ȯ��

int main() {		// ����

	Mat frame, handimg;			// ���
	VideoCapture Cap(0);		// ����
	if (!Cap.isOpened()) {		// ����
		cout << "open failed" << endl;	// ���й�
		return -1;			// ����������
	}

	vector<string> class_names = { "twofinger","threefinger" };		// Ŭ���� �̸�
	auto net = readNetFromDarknet("yolov4-finger.cfg", "yolov4-finger_final.weights");		// �н�,�Ʒõ�����
	net.setPreferableBackend(DNN_BACKEND_OPENCV);		// �鿣�� ����
	net.setPreferableTarget(DNN_TARGET_CPU);					// Ÿ�� ����̽�
	auto output_names = net.getUnconnectedOutLayersNames();	// ��·��̾��̸�

	namedWindow("frame_cap");		// �� ������
	namedWindow("display");			// �� ������

	while (1)		// �ݺ���
	{
		Cap >> frame;	// ���� �б�
		if (frame.empty()) {		// ���ǹ�
			break;		// ����
		}
		Mat blob, conto;		// ���
		vector<Mat> detections;		// ������ mat

		// MedianBlur, Morphology
		Mat open,close;
		morphologyEx(frame, open, MORPH_OPEN, Mat());
		morphologyEx(open, close, MORPH_CLOSE, Mat());
		Mat median_mask;
		medianBlur(close, median_mask, 3);

		
		// YCrCb
		Mat ycrcb;
		cvtColor(frame, ycrcb, COLOR_BGR2YCrCb);
		inRange(ycrcb, Scalar(0, 133, 77), Scalar(255, 173, 127), ycrcb);
		handimg = (ycrcb.size(), CV_8UC3, Scalar(0));
		add(frame, Scalar(0), handimg, ycrcb);
		
		//Contours
		Mat  contours_gray;
		cvtColor(handimg, contours_gray, COLOR_BGR2GRAY);
		vector<vector<Point>> contours;
		findContours(contours_gray, contours, RETR_EXTERNAL, CHAIN_APPROX_NONE);
		cvtColor(contours_gray, conto, COLOR_GRAY2BGR);
		for (int i = 0; i < contours.size(); i++) {
			Scalar c(0, 255, 0);
			drawContours(conto, contours, i, c, 2);
		}
		/* ������ ���� contours�� �ܰ����� �ִ�
		int maxK = 0;
		double maxArea = contourArea(contours[0]);
		for (int k = 1; k < contours.size(); k++)
		{
			double area = contourArea(contours[k]);
			if (area > maxArea)
			{
				maxK = k;
				maxArea = area;
			}
		}
		vector<Point> handContour = contours[maxK];
		*/
		//Convexhull
		vector<vector<Point>>hull(contours.size());
		for (int i = 0; i < contours.size(); i++) {
			convexHull(Mat(contours[i]), hull[i], false);
			Scalar c(255, 0, 0);
			drawContours(conto, hull, i, c, 2);
		}
		/* ������ ���� convexhull���� �ִ밪�� ã�� ��ǥ�� �����Ͽ� �ִ�κп� ���� �׸�
		vector<Vec4i> defects;
		convexityDefects(handContour, hull, defects);
		for (int k = 0; k < defects.size(); k++)
		{
			Vec4i v = defects[k];
			Point ptStart = handContour[v[0]];
			Point ptEnd = handContour[v[1]];
			Point ptFar = handContour[v[2]];
			float depth = v[3] / 256.0;
			if (depth > 10)
			{
				circle(conto, ptStart, 6, Scalar(0, 0, 255), 2);
				circle(conto, ptEnd, 6, Scalar(0, 0, 255), 2);
			}
		}
		*/
	
		// Yolo4 
		blobFromImage(frame, blob, 1 / 255.f, Size(320, 320), Scalar(), true, false, CV_32F);
		net.setInput(blob);
		net.forward(detections, output_names);
		vector<int> indices[NUM_CLASSES];
		vector<Rect> boxes[NUM_CLASSES];
		vector<float> scores[NUM_CLASSES];
		for (auto& output : detections)
		{
			const auto num_boxes = output.rows;
			for (int i = 0; i < num_boxes; i++)
			{
				auto x = output.at<float>(i, 0) * frame.cols;
				auto y = output.at<float>(i, 1) * frame.rows;
				auto width = output.at<float>(i, 2) * frame.cols;
				auto height = output.at<float>(i, 3) * frame.rows;
				Rect rect(x - width / 2, y - height / 2, width, height);
				for (int c = 0; c < NUM_CLASSES; c++)
				{
					auto confidence = *output.ptr<float>(i, 5 + c);
					if (confidence >= CONFIDENCE_THRESHOLD)
					{
						boxes[c].push_back(rect);
						scores[c].push_back(confidence);
					}
				}
			}
		}
		for (int c = 0; c < NUM_CLASSES; c++) {
			NMSBoxes(boxes[c], scores[c], 0.0, NMS_THRESHOLD, indices[c]);		// ������
		}
		// �ٿ�� �ڽ�, Ŭ���� ǥ��
		for (int c = 0; c < NUM_CLASSES; c++)
		{
			for (int i = 0; i < indices[c].size(); ++i)
			{
				const auto color = colors[c % NUM_COLORS];
				auto idx = indices[c][i];
				const auto& rect = boxes[c][idx];
				rectangle(conto, Point(rect.x, rect.y), Point(rect.x + rect.width, rect.y + rect.height), color, 3);
				string label_str = class_names[c] + ": " + format("%.02lf", scores[c][idx]);
				int baseline;
				auto label_bg_sz = getTextSize(label_str, FONT_HERSHEY_COMPLEX_SMALL, 1, 1, &baseline);
				rectangle(conto, Point(rect.x, rect.y - label_bg_sz.height - baseline - 10), Point(rect.x + label_bg_sz.width, rect.y), color, FILLED);
				putText(conto, label_str, Point(rect.x, rect.y - baseline - 5), FONT_HERSHEY_COMPLEX_SMALL, 1, Scalar(0, 0, 0));
			}
		}

		int FPS = CAP_PROP_FPS;			// �����Ӱ�
		String FPSstr = to_string(FPS);		// int to string
		putText(conto, FPSstr, Point(0, 25), FONT_HERSHEY_DUPLEX, 1, Scalar(255, 255, 255));		// ����
		imshow("frame_cap", conto);		// ���� ���

		if (waitKey(10) == 27) {		// ESC
			break;
		}
	}
	destroyAllWindows();		// ��� ������ �ݱ�
	return 0;			// �Լ� ����
}

// ���콺 �̺�Ʈ ����