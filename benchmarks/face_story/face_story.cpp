/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <vector>

#include "ffrt_inner.h"
#include "common.h"

void FaceStory()
{
    PreHotFFRT();

    const int FACE_NUM = 3; // 假设图像上存在多少张人脸，可修改，实际业务可以支持1-10个人脸

    uint32_t inputImageInfo_ = 0; // 模拟输入图像
    std::vector<uint32_t> faceBboxes_; // 模拟人脸检测框
    std::vector<uint32_t> faceDegrees_; // 模拟人脸朝向
    std::vector<uint32_t> faceLandmarks_; // 模拟人脸特征点
    std::vector<uint32_t> faceAttrs_; // 模拟人脸属性
    std::vector<uint32_t> faceMasks_; // 模拟人脸分割
    std::vector<uint32_t> faceAngles_; // 模拟人脸角度

    TIME_BEGIN(t);
    for (uint32_t r = 0; r < REPEAT; r++) {
        for (uint64_t count = 0; count < 10; ++count) {
            // 原图的预处理，输入原始图像，输出预处理图像
            ffrt::submit(
                [&]() {
                    // 下采样Y通道
                    ffrt::submit([&]() { simulate_task_compute_time(COMPUTE_TIME_US); }, {}, {});

                    // 下采样UV通道
                    ffrt::submit([&]() { simulate_task_compute_time(COMPUTE_TIME_US); }, {}, {});

                    ffrt::wait(); // 同步下采样结果

                    // FlushCache
                    simulate_task_compute_time(COMPUTE_TIME_US);
                },
                {}, {&inputImageInfo_});

            // 人脸检测，输入预处理图像，输出人脸检测结果
            ffrt::submit(
                [&]() {
                    faceBboxes_.clear();
                    simulate_task_compute_time(COMPUTE_TIME_US);
                    for (auto i = 0; i < FACE_NUM; i++) {
                        faceBboxes_.push_back(1);
                    }
                },
                {&inputImageInfo_}, {&faceBboxes_});

            // Tracking的Init，对processImageInfo_和faceBboxes_是只读，不存在Rotation的情况下，直接丢出去并发就行，也不需要同步
            ffrt::submit([&]() { simulate_task_compute_time(COMPUTE_TIME_US); }, {&faceBboxes_}, {});

            // 人脸朝向检测，输出人脸朝向结果（省略了根据朝向做旋转）
            faceDegrees_ = std::vector<uint32_t>(FACE_NUM, 0);
            for (auto j = 0; j < FACE_NUM; j++) { // 图像可能存在多个人脸，loop
                ffrt::submit(
                    [&, j]() { // FaceDirectionProcess对于processImageInfo_是只读访问，多个人脸可以并发
                        simulate_task_compute_time(COMPUTE_TIME_US);
                        faceDegrees_[j] = 1;
                    },
                    {&faceBboxes_}, {&faceDegrees_[j]});
            }

            // 人脸特征点检测，输入人脸旋转结果，输出人脸特征点结果
            faceLandmarks_ = std::vector<uint32_t>(FACE_NUM, 0);
            for (auto k = 0; k < FACE_NUM; k++) {
                ffrt::submit(
                    [&, k]() {
                        simulate_task_compute_time(COMPUTE_TIME_US);
                        faceLandmarks_[k] = 1;
                    },
                    {&faceDegrees_[k]}, {&faceLandmarks_[k]});
            }

            // 人脸属性检测，输入人脸旋转结果，输出人脸属性结果
            faceAttrs_ = std::vector<uint32_t>(FACE_NUM, 0);
            for (auto m = 0; m < FACE_NUM; m++) {
                ffrt::submit(
                    [&, m]() {
                        simulate_task_compute_time(COMPUTE_TIME_US);
                        faceAttrs_[m] = 1;
                    },
                    {&faceDegrees_[m]}, {&faceAttrs_[m]});
            }

            // 人脸分割，输入人脸旋转结果，输出人脸特征点结果
            faceMasks_ = std::vector<uint32_t>(FACE_NUM, 0);
            for (auto n = 0; n < 1; n++) { // 实际业务中分割当前只做一次
                ffrt::submit(
                    [&, n]() {
                        simulate_task_compute_time(COMPUTE_TIME_US);
                        faceMasks_[n] = 1;
                    },
                    {&faceDegrees_[n]}, {&faceMasks_[n]});
            }

            // 人脸角度检测，输入人脸特征点结果，输出人脸角度结果
            faceAngles_ = std::vector<uint32_t>(FACE_NUM, 0);
            for (auto q = 0; q < FACE_NUM; q++) { // 实际业务中分割当前只做一次
                ffrt::submit(
                    [&, q]() {
                        simulate_task_compute_time(COMPUTE_TIME_US);
                        faceAngles_[q] = 1;
                    },
                    {&faceLandmarks_[q]}, {&faceAngles_[q]});
            }

            ffrt::wait(); // 同步子任务完成
        }
    }
    TIME_END_INFO(t, "face_story");
}

int main()
{
    GetEnvs();
    FaceStory();
}