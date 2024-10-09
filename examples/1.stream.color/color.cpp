#include <libobsensor/ObSensor.hpp>

#include "utils.hpp"
#include "utils_opencv.hpp"

#include <iostream>
#include <cstdio>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/hwcontext_cuda.h>
#include <libavutil/hwcontext.h>
#include <libavutil/pixdesc.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
}

#include <thread>
#include <mutex>
#include <condition_variable>

#include <fstream>

int index = 0;

std::queue<std::shared_ptr<ob::ColorFrame>> frameQueue;
std::mutex queueMutex;
std::condition_variable queueCondition;

uint64_t color_timestamp_last = 0;
int frameNum = 0;

void saveToBinFile(uint8_t* data, size_t size, const std::string& filename) {
    std::ofstream file(filename, std::ios::binary);
    if (file.is_open()) {
        file.write(reinterpret_cast<char*>(data), size);
        file.close();
        std::cout << "Data saved to " << filename << std::endl;
    } else {
        std::cerr << "Unable to open file " << filename << std::endl;
    }
}

#include <omp.h>

void H264ToRGB(uint8_t* data, unsigned int dataSize, unsigned char* outBuffer) 
{
    // 1. 将元数据装填到packet
    AVPacket* avPkt = av_packet_alloc();
    avPkt->size = dataSize;
    avPkt->data = data;

    static AVCodecContext* codecCtx = nullptr;
    if (codecCtx == nullptr) {
        // 2. 创建并配置硬件加速的codecContext (NVDEC)
        AVCodec* h264Codec = avcodec_find_decoder_by_name("h264_cuvid");
        codecCtx = avcodec_alloc_context3(h264Codec);

        // 2.1 设置用于硬件加速的特定参数
        codecCtx->get_format = [](AVCodecContext* ctx, const enum AVPixelFormat* pix_fmts) {
            // 优先选择cuda格式
            while (*pix_fmts != AV_PIX_FMT_NONE) {
                if (*pix_fmts == AV_PIX_FMT_CUDA) {
                    return AV_PIX_FMT_CUDA;
                }
                pix_fmts++;
            }
            return AV_PIX_FMT_NONE;
        };

        avcodec_open2(codecCtx, h264Codec, nullptr);
    }

    // 3. 解码
    auto ret = avcodec_send_packet(codecCtx, avPkt);
    if (ret >= 0) {
        AVFrame* hwFrame = av_frame_alloc();  // 硬件解码帧
        ret = avcodec_receive_frame(codecCtx, hwFrame);
        if (ret >= 0) {

            // 4. 将GPU上的YUV帧转到CPU上 (解码后仍然在GPU内存中)
            AVFrame* YUVFrame = av_frame_alloc();
            ret = av_hwframe_transfer_data(YUVFrame, hwFrame, 0);
            if (ret < 0) {
                // 错误处理: 无法将帧从GPU传回CPU
                char errbuf[128];
                av_strerror(ret, errbuf, sizeof(errbuf));
                std::cerr << "Error: av_hwframe_transfer_data failed - " << errbuf << std::endl;
                av_frame_free(&hwFrame);
                av_frame_free(&YUVFrame);
                return;
            }

            // 5. YUV 转 RGB24 (使用 NV12 作为源格式)
            AVFrame* RGB24Frame = av_frame_alloc();
            struct SwsContext* convertCxt = sws_getContext(
                YUVFrame->width, YUVFrame->height, AV_PIX_FMT_NV12,
                YUVFrame->width, YUVFrame->height, AV_PIX_FMT_RGB24,
                SWS_BILINEAR, NULL, NULL, NULL
            );

            // 确保数据指针有效
            if (!YUVFrame->data[0] || !YUVFrame->data[1]) {
                std::cerr << "Error: YUVFrame data pointers are null." << std::endl;
                sws_freeContext(convertCxt);
                av_frame_free(&hwFrame);
                av_frame_free(&YUVFrame);
                av_frame_free(&RGB24Frame);
                return;
            }

            // outBuffer将会分配给RGB24Frame->data
            av_image_fill_arrays(
                RGB24Frame->data, RGB24Frame->linesize, outBuffer,
                AV_PIX_FMT_RGB24, YUVFrame->width, YUVFrame->height, 1
            );

            // 使用OpenMP并行处理
            #pragma omp parallel
            {
                #pragma omp single
                {
                    sws_scale(convertCxt, YUVFrame->data, YUVFrame->linesize, 0, YUVFrame->height, RGB24Frame->data, RGB24Frame->linesize);
                }
            }

            // 6. 清理资源
            sws_freeContext(convertCxt);
            av_frame_free(&RGB24Frame);
            av_frame_free(&YUVFrame);
        }
        av_frame_free(&hwFrame);
    }

    av_packet_unref(avPkt);
    av_packet_free(&avPkt);
}











// void H264ToRGB(uint8_t* data, unsigned int dataSize, unsigned char* outBuffer) 
// {
//     // 1. 将元数据装填到packet
//     AVPacket* avPkt = av_packet_alloc();
//     avPkt->size = dataSize;
//     avPkt->data = data;

//     static AVCodecContext* codecCtx = nullptr;
//     if (codecCtx == nullptr) {
//         // 2. 创建并配置硬件加速的codecContext (NVDEC)
//         AVCodec* h264Codec = avcodec_find_decoder_by_name("h264_cuvid");
//         codecCtx = avcodec_alloc_context3(h264Codec);

//         // 2.1 设置用于硬件加速的特定参数
//         codecCtx->get_format = [](AVCodecContext* ctx, const enum AVPixelFormat* pix_fmts) {
//             // 优先选择cuda格式
//             while (*pix_fmts != AV_PIX_FMT_NONE) {
//                 if (*pix_fmts == AV_PIX_FMT_CUDA) {
//                     return AV_PIX_FMT_CUDA;
//                 }
//                 pix_fmts++;
//             }
//             return AV_PIX_FMT_NONE;
//         };

//         avcodec_open2(codecCtx, h264Codec, nullptr);
//     }

//     // 3. 解码
//     auto ret = avcodec_send_packet(codecCtx, avPkt);
//     if (ret >= 0) {
//         AVFrame* hwFrame = av_frame_alloc();  // 硬件解码帧
//         ret = avcodec_receive_frame(codecCtx, hwFrame);
//         if (ret >= 0) {

//             // 4. 将GPU上的YUV帧转到CPU上 (解码后仍然在GPU内存中)
//             AVFrame* YUVFrame = av_frame_alloc();
//             ret = av_hwframe_transfer_data(YUVFrame, hwFrame, 0);
//             if (ret < 0) {
//                 // 错误处理: 无法将帧从GPU传回CPU
//                 char errbuf[128];
//                 av_strerror(ret, errbuf, sizeof(errbuf));
//                 std::cerr << "Error: av_hwframe_transfer_data failed - " << errbuf << std::endl;
//                 av_frame_free(&hwFrame);
//                 av_frame_free(&YUVFrame);
//                 return;
//             }

//             // 检查帧格式
//             // std::cout << "YUVFrame pixel format: " << av_get_pix_fmt_name((AVPixelFormat)YUVFrame->format) << std::endl;

//             // std::cout << "Y plane stride: " << YUVFrame->linesize[0] << std::endl;
//             // std::cout << "UV plane stride: " << YUVFrame->linesize[1] << std::endl;

//             // 5. YUV 转 RGB24 (使用 NV12 作为源格式)
//             AVFrame* RGB24Frame = av_frame_alloc();
//             struct SwsContext* convertCxt = sws_getContext(
//                 YUVFrame->width, YUVFrame->height, AV_PIX_FMT_NV12,  // 使用实际格式 NV12
//                 YUVFrame->width, YUVFrame->height, AV_PIX_FMT_RGB24,
//                 SWS_BILINEAR, NULL, NULL, NULL
//             );
//             // struct SwsContext* convertCxt = sws_getContext(
//             //     YUVFrame->width, YUVFrame->height, AV_PIX_FMT_NV12,  // NV12 源格式
//             //     YUVFrame->width, YUVFrame->height, AV_PIX_FMT_RGB24,  // RGB 目标格式
//             //     SWS_LANCZOS, NULL, NULL, NULL  // 使用更好的缩放算法
//             // );


//             // 确保数据指针有效
//             if (!YUVFrame->data[0] || !YUVFrame->data[1]) {
//                 std::cerr << "Error: YUVFrame data pointers are null." << std::endl;
//                 sws_freeContext(convertCxt);
//                 av_frame_free(&hwFrame);
//                 av_frame_free(&YUVFrame);
//                 av_frame_free(&RGB24Frame);
//                 return;
//             }

//             // outBuffer将会分配给RGB24Frame->data, AV_PIX_FMT_RGB24格式只分配到RGB24Frame->data[0]
//             av_image_fill_arrays(
//                 RGB24Frame->data, RGB24Frame->linesize, outBuffer,
//                 AV_PIX_FMT_RGB24, YUVFrame->width, YUVFrame->height, 1
//             );
//             sws_scale(convertCxt, YUVFrame->data, YUVFrame->linesize, 0, YUVFrame->height, RGB24Frame->data, RGB24Frame->linesize);

//             // std::cout << sizeof(RGB24Frame->data) <<std::endl;
//             // std::cout << sizeof(outBuffer) <<std::endl;
//             // saveToBinFile(RGB24Frame->data, RGB24Frame->w)
//             // auto w = RGB24Frame->width;
//             // auto h = RGB24Frame->height;
//             // auto l = RGB24Frame->linesize[0];
//             // std::cout << w*h*3 << std::endl;

//             // 6. 清理资源
//             sws_freeContext(convertCxt);
//             av_frame_free(&RGB24Frame);
//             av_frame_free(&YUVFrame);
//         }
//         av_frame_free(&hwFrame);
//     }

//     av_packet_unref(avPkt);
//     av_packet_free(&avPkt);
// }

// void H264ToRGB(uint8_t* data, unsigned int dataSize, unsigned char* outBuffer)
// {
//     // 1. 将元数据装填到packet
//     AVPacket* avPkt = av_packet_alloc();
//     avPkt->size = dataSize;
//     avPkt->data = data;

//     static AVCodecContext* codecCtx = nullptr;
//     if (codecCtx == nullptr) {
//         // 2. 创建并配置codecContext
//         AVCodec* h264Codec = avcodec_find_decoder(AV_CODEC_ID_H264);
//         codecCtx = avcodec_alloc_context3(h264Codec);
//         avcodec_get_context_defaults3(codecCtx, h264Codec);
//         avcodec_open2(codecCtx, h264Codec, nullptr);
//     }

//     // 3. 解码
//     //avcodec_decode_video2(codecCtx, &outFrame, &lineLength, &avPkt);  // 接口被弃用，使用下边接口代替
//     auto ret = avcodec_send_packet(codecCtx, avPkt);
//     if (ret >= 0) {
//         AVFrame* YUVFrame = av_frame_alloc();
//         ret = avcodec_receive_frame(codecCtx, YUVFrame);
//         if (ret >= 0) {

//             // 4.YUV转RGB24
//             AVFrame* RGB24Frame = av_frame_alloc();
//             struct SwsContext* convertCxt = sws_getContext(
//                 YUVFrame->width, YUVFrame->height, AV_PIX_FMT_YUV420P,
//                 YUVFrame->width, YUVFrame->height, AV_PIX_FMT_RGB24,
//                 SWS_POINT, NULL, NULL, NULL
//             );

//             // outBuffer将会分配给RGB24Frame->data,AV_PIX_FMT_RGB24格式只分配到RGB24Frame->data[0]
//             av_image_fill_arrays(
//                 RGB24Frame->data, RGB24Frame->linesize, outBuffer,
//                 AV_PIX_FMT_RGB24, YUVFrame->width, YUVFrame->height,
//                 1
//             );
//             sws_scale(convertCxt, YUVFrame->data, YUVFrame->linesize, 0, YUVFrame->height, RGB24Frame->data, RGB24Frame->linesize);

//             // 5.清除各对象/context -> 释放内存
//             // free context and avFrame
//             sws_freeContext(convertCxt);
//             av_frame_free(&RGB24Frame);
//             // RGB24Frame.
//         }
//         // free context and avFrame
//         av_frame_free(&YUVFrame);
//     }
//     // free context and avFrame
//     av_packet_unref(avPkt);
//     av_packet_free(&avPkt);
//     // avcodec_free_context(&codecCtx);
// }

void frameProcess(){
    ob_smpl::CVWindow win("Color");

    while(win.run()) {
        std::unique_lock<std::mutex> lock(queueMutex);
        queueCondition.wait(lock, []{ return!frameQueue.empty(); });
        auto colorFrame = frameQueue.front();
        frameQueue.pop();

        auto width = colorFrame->width();
        auto height = colorFrame->height();
        auto rgb24DataFrame = ob::FrameHelper::createVideoFrame(OB_FRAME_COLOR, OB_FORMAT_RGB, width, height, 0);
        auto rgb24Data = static_cast<uint8_t *>(rgb24DataFrame->data());

        auto data = reinterpret_cast<uint8_t *>(colorFrame->data());
        auto size = colorFrame->dataSize();

        uint64_t color_timestamp_current1 = ob_smpl::getNowTimesMs();
        H264ToRGB(data, size, rgb24Data);
        uint64_t color_timestamp_current2 = ob_smpl::getNowTimesMs();
        std::cout << "color_timestamp_current2 - color_timestamp_current1:" << (color_timestamp_current2 - color_timestamp_current1) << std::endl;

        // frameNum++;
        // uint64_t color_timestamp_current = ob_smpl::getNowTimesMs();
        // auto inteval = color_timestamp_current - color_timestamp_last;
        // if(inteval > 1000){
        //     auto rate =  frameNum * 1000.0 / inteval;
        //     std::cout << "fps:" << rate << std::endl;
        //     frameNum = 0;
        //     color_timestamp_last = color_timestamp_current;
        // }

        // std::cout << sizeof(rgb24Data) <<std::endl;


        // cv::Mat img(height, width, CV_8UC3, rgb24Data);
        // try {
        //     // std::string saveName = "./out/test" + std::to_string(index++);
        //     // std::string saveFormat = ".bin";
        //     // std::string savePath = saveName + saveFormat;
        //     // cv::imwrite(savePath, img);

        //     // saveToBinFile(rgb24Data, sizeof(rgb24Data), savePath);

        //     // cv::namedWindow("Display window", cv::WINDOW_NORMAL);
        //     // cv::resizeWindow("Display window", 800, 600);
        //     cv::imshow("Display window", img);
        // }
        // catch(...) {
        
        // }
    }
}

int main() try {
    // Create a pipeline with default device
    ob::Pipeline pipe;

    // Configure which streams to enable or disable for the Pipeline by creating a Config
    std::shared_ptr<ob::Config> config = std::make_shared<ob::Config>();
    config->enableVideoStream(OB_STREAM_COLOR);

    // Get the color camera configuration list
    try {
        auto                                    colorProfileList = pipe.getStreamProfileList(OB_SENSOR_COLOR);
        std::shared_ptr<ob::VideoStreamProfile> colorProfile     = nullptr;
        if(colorProfileList) {
            // Open the default profile of Color Sensor, which can be configured through the configuration file
            colorProfile = colorProfileList->getVideoStreamProfile(3840, 2160, OB_FORMAT_H264, 25);
            // colorProfile = colorProfileList->getVideoStreamProfile(1920, 1080, OB_FORMAT_H264, 30);
        }
        config->enableStream(colorProfile);
    }
    catch(...) {
        std::cerr << "Current device is not support color sensor!" << std::endl;
    }


    // Start the pipeline with config
    pipe.start(config);

    std::thread consumerThread1(frameProcess);

    // Create a window for rendering and set the resolution of the window.
    // ob_smpl::CVWindow win("Color");

    while(true) {
        // Wait for up to 100ms for a frameset in blocking mode.
        auto frameSet = pipe.waitForFrames(100);
        if(frameSet == nullptr) {
            continue;
        }

        // get color frame from frameset
        auto colorFrame = frameSet->colorFrame();
        if(colorFrame == nullptr) {
            continue;
        }

        std::unique_lock<std::mutex> lock(queueMutex);
        // std::cout << "framesize: " << frameQueue.size() << std::endl;
        if(frameQueue.size() > 50){
            frameQueue.pop();
            std::cout << "pop frame" << std::endl;
        }
        frameQueue.push(colorFrame);
        queueCondition.notify_one();

        // auto width = colorFrame->width();
        // auto height = colorFrame->height();
        // auto rgb24DataFrame = ob::FrameHelper::createVideoFrame(OB_FRAME_COLOR, OB_FORMAT_RGB, width, height, 0);
        // auto rgb24Data = static_cast<uint8_t *>(rgb24DataFrame->data());

        // auto data = reinterpret_cast<uint8_t *>(colorFrame->data());
        // auto size = colorFrame->dataSize();
        // H264ToRGB(data, size, rgb24Data);

        // cv::Mat img(height, width, CV_8UC3, rgb24Data);
        // try {
        //     std::string saveName = "./out/test" + std::to_string(index++);
        //     std::string saveFormat = ".jpg";
        //     std::string savePath = saveName + saveFormat;
        //     cv::imwrite(savePath, img);

        //     // cv::namedWindow("Display window", cv::WINDOW_NORMAL);
        //     // cv::resizeWindow("Display window", 800, 600);
        //     cv::imshow("Display window", img);
        // }
        // catch(...) {
        
        // }

    }

    // Stop the Pipeline, no frame data will be generated
    pipe.stop();

    return 0;
}
catch(ob::Error &e) {
    std::cerr << "function:" << e.getName() << "\nargs:" << e.getArgs() << "\nmessage:" << e.getMessage() << "\ntype:" << e.getExceptionType() << std::endl;
    exit(EXIT_FAILURE);
}
