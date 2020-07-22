#include <opencv2/highgui.hpp>
#include <iostream>

extern "C"
{
#include <libswscale/swscale.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}
#pragma comment(lib, "swscale.lib")
#pragma comment(lib, "avcodec.lib")
#pragma comment(lib, "avutil.lib")
#pragma comment(lib, "avformat.lib")
#pragma comment(lib, "opencv_world320.lib")
using namespace std;
using namespace cv;

int main(int argc, char *argv[]) {
    //海康相机的rtsp url
    const char *inUrl = "rtsp://test:test123456@192.168.1.64";
    //nginx-rtmp rtmp  直播服务器rtmp推流URL
    const char *outUrl = "rtmp://192.168.1.44/live";

    //注册所有网络协议
    avformat_network_init();

    // opencv 接口
    VideoCapture cam;
    Mat frame;
    namedWindow("video");

    //像素格式转换上下文
    SwsContext *vsc = NULL;

    //输出的数据结构
    AVFrame *yuv = NULL;

    //编码器上下文
    AVCodecContext *vc = NULL;

    //rtmp flv 封装器
    AVFormatContext *ic = NULL;

    try {
        /// 1 使用opencv打开rtsp相机
        cam.open(inUrl);
        if (!cam.isOpened()) {
            logic_error ex("cam open failed!");
            throw exception(ex);
        }
        cout << inUrl << " cam open success" << endl;
        int inWidth = cam.get(CAP_PROP_FRAME_WIDTH);
        int inHeight = cam.get(CAP_PROP_FRAME_HEIGHT);
        int fps = cam.get(CAP_PROP_FPS);

        /// 2 初始化格式转换上下文
        vsc = sws_getCachedContext(vsc,
                                   inWidth, inHeight, AV_PIX_FMT_BGR24,     // 源宽、高、像素格式
                                   inWidth, inHeight, AV_PIX_FMT_YUV420P,   // 目标宽、高、像素格式
                                   SWS_BICUBIC,  // 尺寸变化使用算法
                                   0, 0, 0
        );
        if (!vsc) {
            logic_error ex("sws_getCachedContext failed!");
            throw exception(ex);
        }

        /// 3 初始化输出的数据结构
        yuv = av_frame_alloc();
        yuv->format = AV_PIX_FMT_YUV420P;
        yuv->width = inWidth;
        yuv->height = inHeight;
        yuv->pts = 0;

        // 配yuv空间
        int ret = av_frame_get_buffer(yuv, 32);
        if (ret != 0) {
            char buf[1024] = {0};
            av_strerror(ret, buf, sizeof(buf) - 1);
            logic_error ex(buf);
            throw exception(ex);
        }

        /// 4 初始化编码上下文
        // a 找到编码器
        AVCodec *codec = avcodec_find_encoder(AV_CODEC_ID_H264);
        if (!codec) {
            logic_error ex("Can`t find h264 encoder!");
            throw exception(ex);
        }

        // b 创建编码器上下文
        vc = avcodec_alloc_context3(codec);
        if (!vc) {
            logic_error ex("avcodec_alloc_context3 failed!");
            throw exception(ex);
        }

        // c 配置编码器参数
        vc->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;   // 全局参数
        vc->codec_id = codec->id;
        vc->thread_count = 8;

        vc->bit_rate = 50 * 1024 * 8;    // 压缩后每秒视频的bit位大小 50kB
        vc->width = inWidth;
        vc->height = inHeight;
        vc->time_base = {1, fps};
        vc->framerate = {fps, 1};

        // 画面组的大小，多少帧一个关键帧
        vc->gop_size = 50;
        vc->max_b_frames = 0;
        vc->pix_fmt = AV_PIX_FMT_YUV420P;

        // d 打开编码器上下文
        ret = avcodec_open2(vc, 0, 0);
        if (ret != 0) {
            char buf[1024] = {0};
            av_strerror(ret, buf, sizeof(buf) - 1);
            logic_error ex(buf);
            throw exception(ex);
        }
        cout << "avcodec_open2 success!" << endl;

        /// 5 输出封装器和视频流配置
        // a 创建输出封装器上下文
        ret = avformat_alloc_output_context2(&ic, 0, "flv", outUrl);
        if (ret != 0) {
            char buf[1024] = {0};
            av_strerror(ret, buf, sizeof(buf) - 1);
            logic_error ex(buf);
            throw exception(ex);
        }

        // b 添加视频流
        AVStream *vs = avformat_new_stream(ic, NULL);
        if (!vs) {
            logic_error ex("avformat_new_stream failed");
            throw exception(ex);
        }
        vs->codecpar->codec_tag = 0;

        // 从编码器复制参数
        avcodec_parameters_from_context(vs->codecpar, vc);
        av_dump_format(ic, 0, outUrl, 1);

        /// 6 打开rtmp 的网络输出IO
        ret = avio_open(&ic->pb, outUrl, AVIO_FLAG_WRITE);
        if (ret != 0) {
            char buf[1024] = {0};
            av_strerror(ret, buf, sizeof(buf) - 1);
            logic_error ex(buf);
            throw exception(ex);
        }

        // 写入封装头
        ret = avformat_write_header(ic, NULL);
        if (ret != 0) {
            char buf[1024] = {0};
            av_strerror(ret, buf, sizeof(buf) - 1);
            logic_error ex(buf);
            throw exception(ex);
        }

        AVPacket pack;
        memset(&pack, 0, sizeof(pack));
        int vpts = 0;
        for (;;) {
            /// 取rtsp视频帧，解码视频帧
            if (!cam.grab()) {
                continue;
            }

            /// yuv转换为rgb
            if (!cam.retrieve(frame)) {
                continue;
            }
            //imshow("video", frame);
            //waitKey(1);

            /// rgb to yuv
            // 输入的数据结构
            uint8_t *indata[AV_NUM_DATA_POINTERS] = {0};
            //indata[0] bgrbgrbgr
            //plane indata[0] bbbbb indata[1]ggggg indata[2]rrrrr
            indata[0] = frame.data;
            int insize[AV_NUM_DATA_POINTERS] = {0};

            // 一行（宽）数据的字节数
            insize[0] = frame.cols * frame.elemSize();
            int h = sws_scale(vsc, indata, insize, 0, frame.rows,  // 源数据
                              yuv->data, yuv->linesize);
            if (h <= 0) {
                continue;
            }
            //cout << h << " " << flush;

            /// h264
            yuv->pts = vpts;
            vpts++;
            ret = avcodec_send_frame(vc, yuv);
            if (ret != 0)
                continue;

            ret = avcodec_receive_packet(vc, &pack);
            if (ret != 0 || pack.size > 0) {
                //cout << "*" << pack.size << flush;
            } else {
                continue;
            }

            // 推流
            pack.pts = av_rescale_q(pack.pts, vc->time_base, vs->time_base);
            pack.dts = av_rescale_q(pack.dts, vc->time_base, vs->time_base);
            pack.duration = av_rescale_q(pack.duration, vc->time_base, vs->time_base);
            ret = av_interleaved_write_frame(ic, &pack);
            if (ret == 0) {
                cout << "#" << flush;
            }
        }
    }
    catch (exception &ex) {
        if (cam.isOpened())
            cam.release();

        if (vc) {
            avio_closep(&ic->pb);
            avcodec_free_context(&vc);
        }

        cerr << ex.what() << endl;
    }
    getchar();
    return 0;
}