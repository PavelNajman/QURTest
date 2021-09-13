/*
 *  Copyright (c) 2021 Pavel Najman
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy of this software
 *  and associated documentation files (the "Software"), to dea in the Software without
 *  restriction, including without limitation the rights to use, copy, modify, merge, publish,
 *  distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the
 *  Software is furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in all copies or
 *  substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
 *  BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 *  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 *  DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 *  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <algorithm>
#include <iostream>
#include <cmath>

#include <iterator>
#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/opencv.hpp>

#include <qrencode.h>

#include <bc-ur/bc-ur.hpp>
#include <bc-ur/ur-encoder.hpp>

#include <lifehash.hpp>

/**
 *  \brief  Holds command line arguments.
 */
struct CommandLineArguments
{
    /// Generate single part UR flag.
    bool isSinglePart = true;
    /// Generated message length in bytes.
    size_t messageLength = 100;
    /// Maximum fragment length for multi-part UR in bytes.
    size_t maxFragmentLength = 100;
    /// Number of extra parts for multi-part UR.
    size_t numExtraParts = 0;
    /// Size of generated QR image in pixels
    int qrSize = 256;
    /// Size of generated Lifehash image in pixels.
    int lifeHashImageSize = 128;
    /// Number of FPS for multi-part QR code visualization.
    int fps = 4;
};

/**
 *  \brief  Parses command line arguments.
 *  \param  argc    Number of command line arguments.
 *  \param  argv    Array of command line strings.
 *  \returns    Parsed command line arguments.
 */
static CommandLineArguments ParseCommandLineArguments(const int argc, char** argv)
{
    CommandLineArguments result;
    for (int i = 1; i < argc; ++i)
    {
        const auto arg = std::string(argv[i]);
        if (arg == "-m")
        {
            result.isSinglePart = false;
        }
        else if (arg == "-l")
        {
            assert(i+1 <= argc && "Value expected.");
            result.messageLength = stoul(std::string(argv[++i]));
        }
        else if (arg == "-f")
        {
            assert(i+1 <= argc && "Value expected.");
            result.maxFragmentLength = stoul(std::string(argv[++i]));
        }
        else if (arg == "-e")
        {
            assert(i+1 <= argc && "Value expected.");
            result.numExtraParts = stoul(std::string(argv[++i]));
        }
        else if (arg == "-h")
        {
            std::cerr << "Usage: ./qurtest [OPTION]..." << std::endl;
            std::cerr << "\t-h\tPrint help and exist." << std::endl;
            std::cerr << "\t-m\tGenerate multi-part UR (default=false)." << std::endl;
            std::cerr << "\t-l <value>\tByte length of the generated data (default=100)." << std::endl;
            std::cerr << "\t-f <value>\tByte length of a single data fragment in a multi-part UR (default=100)." << std::endl;
            std::cerr << "\t-e <value>\tNumber of extra parts in a multi-part UR (default=0)." << std::endl;
            std::cerr << "\t-s <value>\tSize of the generated QR image (default=256px)." << std::endl;
            std::cerr << "\t-t <value>\tNumber of FPS for multi-part QUR visualization.  (default=4)." << std::endl;
            exit(0);
        }
        else if (arg == "-s")
        {
            assert(i+1 <= argc && "Value expected.");
            result.qrSize = stoul(std::string(argv[++i]));
        }
        else if (arg == "-t")
        {
            assert(i+1 <= argc && "Value expected.");
            result.fps = stoul(std::string(argv[++i]));
        }
        else
        {
            assert(false && "Unexpected command line argument");
        }
    }
    
    const size_t MAX_LENGTH = 2956 / 2 - 13;
    if (result.isSinglePart)
    {
        assert(result.messageLength <= MAX_LENGTH && "Message too long for single part UR");
    }
    else
    {
        assert(result.messageLength >= result.maxFragmentLength && result.maxFragmentLength <= MAX_LENGTH && "Fragment too long");
    }

    return result;
}

/**
 *  \brief  Generates a random message with a given length.
 *  \param  len Length of a generated message in bytes.
 *  \returns    Generated message.
 */
static ur::ByteVector MakeMessage(const size_t len)
{
    auto rng = ur::Xoshiro256(time(nullptr));
    return rng.next_data(len);
}

/**
 *  \brief  Generates a random message with a given length and stores it as a UR object.
 *  \param  len Lengths of a generated message in bytes.
 *  \returns    UR object that containt the generated message.
 */
static ur::UR MakeMessageUr(const size_t len)
{
    const auto message = MakeMessage(len);
    ur::ByteVector cbor;
    ur::CborLite::encodeBytes(cbor, message);
    return ur::UR("bytes", cbor);
}

/**
 *  \brief  Encodes the given message as a single part UR.
 *  \param  message A message that will be encoded.
 *  \returns    UR encoded string.
 */
static std::string GenerateSinglePartUr(const ur::UR& message)
{
    return ur::UREncoder::encode(message);
}

/**
 *  \brief  Encodes the given message as a multi-part UR.
 *  \param  message A message that will be encoded.
 *  \param  maxFragmentLen  Maximum length of a fragment in bytes.
 *  \param  numExtraParts   Number of extra fragments.
 *  \returns    UR encoded strings.
 */
static std::vector<std::string> GenerateMultiPartUr(const ur::UR& message, const size_t maxFragmentLen, const size_t numExtraParts = 0)
{
    auto encoder = ur::UREncoder(message, maxFragmentLen);

    std::vector<std::string> result;
    for (size_t i = 0; i < encoder.seq_len() + numExtraParts; ++i)
    {
        result.emplace_back(encoder.next_part());
    }
    return result;
}

/**
 *  \brief  Creates a lifehash image of a given message.
 *  \param  message Message whose lifehash image will be computed.
 *  \param  size    Size of the created lifehash image.
 *  \returns    Lifehash image.
 */
static cv::Mat CreateLifeHashImage(const ur::UR& message, const int size)
{
    const auto lifeHash = LifeHash::make_from_data(message.cbor());
    cv::Mat lifeHashImage(cv::Size(lifeHash.width, lifeHash.height), CV_8UC3);
    for (size_t r = 0; r < lifeHashImage.rows; ++r)
    {
        for (size_t c = 0; c <lifeHashImage.cols; ++c)
        {
            const auto i = 3 * (r * lifeHash.width + c);
            lifeHashImage.at<cv::Vec3b>(r, c) = cv::Vec3b(lifeHash.colors[i+2], lifeHash.colors[i+1], lifeHash.colors[i]);
        }
    }
    cv::resize(lifeHashImage, lifeHashImage, cv::Size(size, size), 0, 0, cv::INTER_NEAREST);
    return lifeHashImage;
}

/**
 *  \brief  UR encodes the given message.
 *  \param  message A message that will be encoded.
 *  \param  args    Command line arguments.
 *  \returns    A vector of UR encoded strings.
 */
static std::vector<std::string> CreateUrs(const ur::UR& message, const CommandLineArguments& args)
{
    if (args.isSinglePart)
    {
        return {GenerateSinglePartUr(message)};
    }
    return GenerateMultiPartUr(message, args.maxFragmentLength, args.numExtraParts);
}

/**
 *  \brief  Creates QR images that contains UR encoded strings.
 *  \param  urs A vector of UR encoded strings.
 *  \param  size    Size of the created QR images.
 *  \returns    A vector of QR images.
 */
static std::vector<cv::Mat> CreateQurImages(const std::vector<std::string>& urs, const int size)
{
    std::vector<QRcode*> qurs;
    std::transform(urs.begin(), urs.end(), std::back_inserter(qurs), [](const auto& ur){ return QRcode_encodeString8bit(ur.c_str(), 0, QR_ECLEVEL_L); });

    std::vector<cv::Mat> qurImages;
    for (const auto qur : qurs)
    {
        qurImages.emplace_back(qur->width, qur->width, CV_8U);
        for (int r = 0; r < qurImages.back().rows; ++r)
        {
            for (int c = 0; c < qurImages.back().cols; ++c)
            {
                qurImages.back().at<uchar>(r, c) = (*(qur->data + r * qurImages.back().rows + c) % 2 == 1) ? 0 : 255;
            }
        }
        cv::resize(qurImages.back(), qurImages.back(), cv::Size(size, size), 0, 0, cv::INTER_NEAREST);
        cv::cvtColor(qurImages.back(), qurImages.back(), cv::COLOR_GRAY2BGR);
    }

    return qurImages;
}

/**
 *  \brief  Shows the lifehash and the QR images.
 *  \param  lifeHashImage   A lifehash image of a message.
 *  \param  qurImages   A vector of QR images that contain UR encoded message.
 */
static void Present(const cv::Mat& lifeHashImage, const std::vector<cv::Mat>& qurImages, const int fps)
{
    int size = lifeHashImage.cols;
    for (const auto& img : qurImages)
    {
        size = std::max(img.cols, size);
    }

    std::vector<cv::Mat> images;
    for (const auto& qurImage : qurImages)
    {
        const int MARGIN = 10;
        images.emplace_back(cv::Size(2*MARGIN + size, 3*MARGIN + lifeHashImage.rows + size), CV_8UC3, cv::Scalar(255, 255, 255));
        
        const cv::Rect lifeHashRoi((images.back().cols - lifeHashImage.cols) >> 1, MARGIN, lifeHashImage.cols, lifeHashImage.rows);
        lifeHashImage.copyTo(images.back()(lifeHashRoi));

        const cv::Rect qurImageRoi((images.back().cols - qurImage.cols) >> 1, 2*MARGIN + lifeHashImage.rows, qurImage.cols, qurImage.rows);
        qurImage.copyTo(images.back()(qurImageRoi));
    }

    int i = 0;
    while (cv::waitKey(1000.0 / fps) != 27)
    {
        cv::imshow("QUR", images[i]);
        i = (++i) % images.size();
    }

}

int main(int argc, char** argv)
{
    const auto args = ParseCommandLineArguments(argc, argv);

    const auto message = MakeMessageUr(args.messageLength);
    
    const auto lifeHashImage = CreateLifeHashImage(message, args.lifeHashImageSize);

    const auto urs = CreateUrs(message, args);

    const auto qurImages = CreateQurImages(urs, args.qrSize);

    Present(lifeHashImage, qurImages, args.fps);

    return 0;
}

