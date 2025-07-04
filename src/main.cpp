#include <opencv2/videoio.hpp>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize.h"

#include <iostream>
#include <vector>
#include <cmath>
#include <cstring>
#include <opencv2/opencv.hpp>

#define BLOCK_SIZE 8
#define K1 0.01
#define K2 0.03
#define L 255
float C1 = std::pow(K1 * L, 2);
float C2 = std::pow(K2 * L, 2);
float C3 = C2 / 2;

void
rgb2gray(unsigned char *img, int width, int height, int channels,
         std::vector<unsigned char> &out) {
    for(int i = 0; i < width * height; i++) {
        float r = img[i * channels + 0];
        float g = img[i * channels + 1];
        float b = img[i * channels + 2];

        out[i] =
            static_cast<unsigned char>(0.299f * r + 0.587f * g + 0.114f * b);
    }
}

inline float
calc_luminance(float mu_x, float mu_y) {
    return (2 * mu_x * mu_y + C1) /
           (std::pow(mu_x, 2) + std::pow(mu_y, 2) + C1);
}

inline float
calc_contrast(float var_x, float var_y) {
    return (2 * std::sqrt(var_x) * std::sqrt(var_y) + C2) /
           (var_x + var_y + C2);
}

inline float
calc_structure(float cov_xy, float var_x, float var_y) {
    float sig_x = std::sqrt(var_x);
    float sig_y = std::sqrt(var_y);

    return (cov_xy + C3) / (sig_x * sig_y + C3);
}
float
ssim(const std::vector<unsigned char> &img1,
     const std::vector<unsigned char> &img2, int width, int height) {
    std::vector<unsigned char> img1_data(0);
    std::vector<unsigned char> img2_data(0);
    std::vector<float> ssims{};

    for(int i = 0; i < height; i += BLOCK_SIZE) {
        for(int j = 0; j < width; j += BLOCK_SIZE) {
            int sum1 = 0;
            int sum2 = 0;
            float mean1 = 0;
            float mean2 = 0;
            float var1 = 0;
            float var2 = 0;
            img1_data.clear();
            img2_data.clear();

            // Fill in vectors
            for(int k = 0; k < BLOCK_SIZE; k++) {
                for(int l = 0; l < BLOCK_SIZE; l++) {
                    img1_data.push_back(
                        img1[i * width + k * BLOCK_SIZE + j + l]);
                    img2_data.push_back(
                        img2[i * width + k * BLOCK_SIZE + j + l]);
                }
            }

            // calculate averages
            for(int k = 0; k < std::pow(BLOCK_SIZE, 2); k++) {
                sum1 += img1_data.at(k);
                sum2 += img2_data.at(k);
            }
            mean1 = sum1 /= std::pow(BLOCK_SIZE, 2);
            mean2 = sum2 /= std::pow(BLOCK_SIZE, 2);

            // calculate variances
            for(int k = 0; k < std::pow(BLOCK_SIZE, 2); k++) {
                var1 += std::pow(img1_data.at(k) - mean1, 2);
                var2 += std::pow(img2_data.at(k) - mean2, 2);
            }
            var1 /= std::pow(BLOCK_SIZE, 2) - 1;
            var2 /= std::pow(BLOCK_SIZE, 2) - 1;

            // calculate covariance
            float cov = 0;
            for(int k = 0; k < std::pow(BLOCK_SIZE, 2); k++) {
                cov += (img1_data.at(k) - mean1) * (img2_data.at(k) - mean2);
            }
            cov /= std::pow(BLOCK_SIZE, 2) - 1;

            float luminance = calc_luminance(mean1, mean2);
            float contrast = calc_contrast(var1, var2);
            float structure = calc_structure(cov, var1, var2);

            ssims.push_back(luminance * contrast * structure);
            // std::cout << "SSIM for this block: "
            //           << luminance * contrast * structure << "\n";
        }
    }
    float sum = 0;
    for(auto &i: ssims) {
        sum += i;
    }
    return sum / ssims.size();
}

std::vector<unsigned char>
mat2gray(const cv::Mat &frame) {
    cv::Mat gray;
    cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);

    std::vector<unsigned char> pixel_data;
    pixel_data.assign(gray.data, gray.data + gray.total());
    return pixel_data;
}

int
main(int argc, char *argv[]) {
    if(argc != 3) {
        std::cerr << "Usage: " << argv[0]
                  << " <video_path> <last_frame_path>\n";
        return 1;
    }
    const int frames_to_read = 100;

    const char *video_path = argv[1];
    const char *last_frame_path = argv[2];

    cv::VideoCapture cap(video_path);
    if(!cap.isOpened()) {
        std::cerr << "Error: Could not open video file\n";
        return 1;
    }
    double fps = cap.get(cv::CAP_PROP_FPS);
    int frame_width = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_WIDTH));
    int frame_height = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_HEIGHT));

    cv::Mat curr_frame;
    // std::vector<cv::Mat> frames;
    // frames.reserve(frames_to_read);
    int frames_read = 0;

    while(frames_read < frames_to_read) {
        bool ret = cap.read(curr_frame);
        if(!ret) {
            std::cout << "Reached end of video or error reading frame at frame "
                      << frames_read << std::endl;
            break;
        }

        int frame_channels = curr_frame.channels();
        std::vector<unsigned char> gray_frame = mat2gray(curr_frame);
        int rf_width, rf_height, rf_channels;
        unsigned char *rf =
            stbi_load(last_frame_path, &rf_width, &rf_height, &rf_channels, 0);

        if(!rf) {
            std::cerr << "Error: Could not load image " << last_frame_path
                      << std::endl;
            return 1;
        }
        // Make sure both images are the same size
        int op_width = 0;
        int op_height = 0;

        if(rf_width != frame_width) {
            // std::cout << "Reference Image has width of " << rf_width
            //           << " and Video Frame has width of " << frame_width
            //           << "\n ";
            if(rf_width < frame_width)
                op_width = rf_width;
            else
                op_width = frame_width;
        } else {
            op_width = rf_width;
            std::cout << "Set Width to " << op_width << "\n";
        }

        if(rf_height != frame_height) {
            // std::cout << "Reference Image has has height of " << rf_height
            //           << " and Video Frame has height of " << frame_height
            //           << "\n ";
            if(rf_height < frame_height)
                op_height = rf_height;
            else
                op_height = frame_height;
        } else {
            op_height = rf_height;
            std::cout << "Set Height to " << op_width << "\n";
        }
        unsigned char *op_img1 =
            new unsigned char[rf_width * rf_height * rf_channels];
        unsigned char *op_img2 =
            new unsigned char[frame_width * frame_height * frame_channels];

        if(rf_height != op_height || rf_width != op_width) {
            stbir_resize_uint8(rf, rf_width, rf_height, 0, op_img1, op_width,
                               op_height, 0, rf_channels);
        } else {
            std::memcpy(op_img1, rf, rf_width * rf_height * rf_channels);
        }
        if(frame_height != op_height || frame_width != op_width) {
            stbir_resize_uint8(gray_frame.data(), frame_width, frame_height, 0,
                               op_img2, op_width, op_height, 0, frame_channels);
        } else {
            std::memcpy(op_img2, gray_frame.data(),
                        frame_width * frame_height * frame_channels);
        }

        stbi_image_free(rf);
        std::vector<unsigned char> rfbw(op_width * op_height);

        rgb2gray(op_img1, op_width, op_height, rf_channels, rfbw);

        stbi_image_free(op_img1);
        stbi_image_free(op_img2);

        float res = ssim(rfbw, gray_frame, op_width, op_height);
        std::cout << "mu_SSIM: " << res << "\n";

        frames_read++;
    }

    cap.release();

    //   int i1_width, i1_height, i1_channels;
    //   unsigned char *img1 =
    //       stbi_load(video_path, &i1_width, &i1_height, &i1_channels, 0);
    //
    //   if(!img1) {
    //       std::cerr << "Error: Could not load image " << video_path <<
    //       std::endl; return 1;
    //   }
    //
    //   int i2_width, i2_height, i2_channels;
    //   unsigned char *img2 =
    //       stbi_load(last_frame_path, &i2_width, &i2_height, &i2_channels, 0);
    //   if(!img2) {
    //       std::cerr << "Error: Could not load image " << last_frame_path <<
    //       std::endl; return 1;
    //   }
    //
    //   // Make sure both images are the same size
    //   int op_width = 0;
    //   int op_height = 0;
    //
    //   if(i1_width != i2_width) {
    // std::cout<<"Image 1 has width of " << i1_width << " and Image 2 has width
    // of " << i2_width <<"\n";
    //       if(i1_width < i2_width)
    //           op_width = i1_width;
    //       else
    //           op_width = i2_width;
    //   } else {
    //       op_width = i1_width;
    // std::cout<<"Set Width to " << op_width <<"\n";
    //   }
    //
    //   if(i1_height != i2_height) {
    // std::cout<<"Image 1 has height of " << i1_height << " and Image 2 has
    // height of " << i2_height <<"\n";
    //       if(i1_height < i2_height)
    //           op_height = i1_height;
    //       else
    //           op_height = i2_height;
    //   } else {
    //       op_height = i1_height;
    // std::cout<<"Set Height to " << op_width <<"\n";
    //   }
    //   unsigned char *op_img1 =
    //       new unsigned char[i1_width * i1_height * i1_channels];
    //   unsigned char *op_img2 =
    //       new unsigned char[i2_width * i2_height * i2_channels];
    //
    //   if(i1_height != op_height || i1_width != op_width) {
    //       stbir_resize_uint8(img1, i1_width, i1_height, 0, op_img1, op_width,
    //                          op_height, 0, i1_channels);
    //   } else {
    //       std::memcpy(op_img1, img1, i1_width * i1_height * i1_channels);
    //   }
    //   if(i2_height != op_height || i2_width != op_width) {
    //       stbir_resize_uint8(img2, i2_width, i2_height, 0, op_img2, op_width,
    //                          op_height, 0, i2_channels);
    //   } else {
    //       std::memcpy(op_img2, img2, i2_width * i2_height * i2_channels);
    //   }
    //
    //   stbi_image_free(img1);
    //   stbi_image_free(img2);
    //
    //   std::vector<unsigned char> im1bw(op_width * op_height);
    //   std::vector<unsigned char> im2bw(op_width * op_height);
    //
    //   rgb2gray(op_img1, op_width, op_height, i1_channels, im1bw);
    //   rgb2gray(op_img2, op_width, op_height, i2_channels, im2bw);
    //
    //   stbi_image_free(op_img1);
    //   stbi_image_free(op_img2);
    //
    //   float res = ssim(im1bw, im2bw, i1_width, i1_height);
    //   std::cout << "mu_SSIM: " << res << "\n";

    return 0;
}
