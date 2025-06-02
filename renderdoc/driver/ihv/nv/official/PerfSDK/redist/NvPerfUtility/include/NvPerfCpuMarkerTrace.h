/*
* Copyright 2014-2025 NVIDIA Corporation.  All rights reserved.
*
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

#pragma once

#include <cstdint>
#include <cstring>
#include <vector>

namespace nv { namespace perf {

    template <class FrameUserData>
    class CpuMarkerTrace
    {
    public:
        // parameter struct that will be passed to the callback given to UpdateCurrentFrameUserData
        struct FrameUserDataFnParams
        {
            size_t validMarkerCount;
            size_t droppedMarkerCount;
            size_t droppedCharCount;
            FrameUserData* pUserData;
        };

        struct Marker
        {
            const char* pName; // null-terminated string
        };

        struct FrameMarkers
        {
            size_t validMarkerCount;
            size_t droppedMarkerCount;
            size_t droppedNameCharCount;
            const Marker* pBegin;
            const Marker* pEnd;
            const FrameUserData* pUserData;
        };

    private:
        // internal data stored for each frame
        struct Frame
        {
            size_t validMarkerCount;
            size_t droppedMarkerCount;
            size_t totalNameCharCount;
            FrameUserData userData;
        };

        // Vector is used to allocate a buffer to store all ring buffers.
        // If a buffer is passed in Initialize, this vector will remain default constructed
        std::vector<uint8_t> m_storage;

        // Ring buffers are strictly partitioned per frame, to provide QoS.
        Frame* m_pFramesRing;
        Marker* m_pMarkersRing;
        char* m_pNamesRing;

        // size of each of the individual buffers
        size_t m_maxFrames;
        size_t m_maxMarkersPerFrame;
        size_t m_maxNameCharsPerFrame;

        // write indices
        size_t m_writeFrameIndex;
        size_t m_writeMarkerIndex;
        size_t m_writeNameIndex;

        // These cache the address of the start of each array for the current frame.
        char* m_pFrameNames;
        Marker* m_pFrameMarkers;

        // read index
        size_t m_readFrameIndex;

        // Counter is used to differentiate between empty and full case when indices are equal.
        size_t m_unreadFrameCount;

        // This is set when PushMarker is called and the frame ring buffer is full, and cleared when OnFrameEnd is called.
        // TRUE indicates that all markers for the current frame should be dropped, which avoids markers being partially recorded
        // if the frame ring buffer becomes not-full in the middle of a frame.
        bool m_frameInvalid;

        static size_t CircularIncrement(size_t index, size_t max)
        {
            return (++index >= max) ? 0 : index;
        }

    public:
        // Use this to see the total number of bytes required for a pre-allocated buffer to use with Initialize().
        static size_t GetTotalMemoryUsage(size_t maxFrames, size_t maxMarkersPerFrame, size_t maxNameCharsPerFrame)
        {
            return maxFrames * (sizeof(Frame) + (maxMarkersPerFrame * sizeof(Marker)) + (maxNameCharsPerFrame * sizeof(char)));
        }

        // This function gets the number of markers dropped due to overruns in the current frame.
        size_t GetCurrentFrameDroppedMarkerCount() const
        {
            const Frame& frame = m_pFramesRing[m_writeFrameIndex];
            return frame.droppedMarkerCount;
        }

        // This returns the number of frames still in the frame ring buffer; they can be cleared with ReleaseOldestFrame().
        size_t GetUnreadFrameCount() const
        {
            return m_unreadFrameCount;
        }

        // Initializes all data structures required for this class, using pre-allocated memory
        // Initialize() must be called before calling any other function in the class.
        // The size of buffer that is required is given by GetTotalMemoryUsage().
        // This class does NOT take ownership of pStorage.
        void Initialize(size_t maxFrames, size_t maxMarkersPerFrame, size_t maxNameCharsPerFrame, uint8_t* pStorage)
        {
            m_pFramesRing = reinterpret_cast<Frame*>(pStorage);
            m_pMarkersRing = reinterpret_cast<Marker*>(pStorage + maxFrames * sizeof(Frame));
            m_pNamesRing = reinterpret_cast<char*>(pStorage + maxFrames * (sizeof(Frame) + maxMarkersPerFrame * sizeof(Marker)));
            m_maxFrames = maxFrames;
            m_maxMarkersPerFrame = maxMarkersPerFrame;
            m_maxNameCharsPerFrame = maxNameCharsPerFrame;
            m_writeFrameIndex = 0;
            m_writeMarkerIndex = 0;
            m_writeNameIndex = 0;
            m_pFrameMarkers = m_pMarkersRing + m_writeFrameIndex * m_maxMarkersPerFrame;
            m_pFrameNames = m_pNamesRing + m_writeFrameIndex * m_maxNameCharsPerFrame;
            m_readFrameIndex = 0;
            m_unreadFrameCount = 0;
            m_frameInvalid = false;

            m_pFramesRing[m_writeFrameIndex] = {};
        }

        // Initializes all data structures required for this class, using class-owned memory.
        // Initialize() must be called before calling any other function in the class
        void Initialize(size_t maxFrames, size_t maxMarkersPerFrame, size_t maxNameCharsPerFrame)
        {
            m_storage = std::move(std::vector<uint8_t>(GetTotalMemoryUsage(maxFrames, maxMarkersPerFrame, maxNameCharsPerFrame)));
            return Initialize(maxFrames, maxMarkersPerFrame, maxNameCharsPerFrame, m_storage.data());
        }

        // Resets this object to its default state.
        // Initialize() must be called before using this object again.
        void Reset()
        {
            m_pFramesRing = nullptr;
            m_pMarkersRing = nullptr;
            m_pNamesRing = nullptr;
            m_unreadFrameCount = 0;
            m_storage = std::move(std::vector<uint8_t>());
        }

        // Returns true on successful recording of the Marker; returns false if data was dropped.
        // Data will be dropped if the object has not been initialized with Initialize(), or if any of the ring buffers are full.
        // Length can be assigned to strlen(pName) by the caller, if it is known; else pass in zero.
        // If passed in length is 0, pName must be a null terminated string.
        // pName must not be nullptr.
        bool PushMarker(const char* pName, const size_t length)
        {
            // Initialization has not run yet.
            if (!m_pFramesRing)
            {
                return false;
            }

            // If the buffer is completely full, or was full earlier in this frame, drop all markers and do not write to the current write index.
            if (m_frameInvalid)
            {
                return false;
            }
            const size_t nameLength = length ? length : std::strlen(pName);
            Frame& frame = m_pFramesRing[m_writeFrameIndex];

            // Record the total chars, even if this marker is dropped,
            // so the number reported as dropped chars is correct.
            frame.totalNameCharCount += nameLength + 1;

            // Bounds check write pointers within current frame; if no space, then drop the Marker.
            // If a marker was previously dropped this frame, all subsequent shall be dropped as well.
            if (frame.droppedMarkerCount || m_writeMarkerIndex == m_maxMarkersPerFrame || m_writeNameIndex + nameLength + 1 > m_maxNameCharsPerFrame)
            {
                frame.droppedMarkerCount += 1;
                return false;
            }

            // record the marker name in the current name buffer
            char* pNameDest = m_pFrameNames + m_writeNameIndex;
            std::memcpy(pNameDest, pName, nameLength);
            pNameDest[nameLength] = '\0';
            m_writeNameIndex += nameLength + 1;

            // record the marker in the current marker buffer, and set pointer to location in name buffer
            Marker& marker = m_pFrameMarkers[m_writeMarkerIndex];
            marker.pName = pNameDest;
            m_writeMarkerIndex += 1;

            frame.validMarkerCount += 1;

            return true;
        }

        bool PushMarker(const char* pName)
        {
            return PushMarker(pName, std::strlen(pName));
        }

        // This function should be called by the user once a frame is finished.
        // This advances to the next frame's storage and saves this frame's data for later retreival in GetOldestFrameMarkers().
        // Return value is false if the write pointer could not be advanced (ring buffers are full), and
        // subsequent calls to PushMarker will fail until OnFrameEnd is called with space available.
        bool OnFrameEnd()
        {
            // If the current frame was dropped, no new information has been written, so there are no new unread frames.
            if (m_frameInvalid)
            {
                m_frameInvalid = false;
            }
            else
            {
                m_unreadFrameCount += 1;
            }

            // If the frame buffer is full, there is no way to advance the write pointer. Set flag to show that no markers should be written.
            if (m_unreadFrameCount == m_maxFrames)
            {
                m_frameInvalid = true;
                return false;
            }

            m_writeFrameIndex = CircularIncrement(m_writeFrameIndex, m_maxFrames);

            // Clear the new frame.  Note this is O(1).
            m_pFramesRing[m_writeFrameIndex] = {};
            m_writeMarkerIndex = 0;
            m_writeNameIndex = 0;

            m_pFrameNames = m_pNamesRing + m_writeFrameIndex * m_maxNameCharsPerFrame;
            m_pFrameMarkers = m_pMarkersRing + m_writeFrameIndex * m_maxMarkersPerFrame;

            return true;
        }

        // Returns information from the oldest stored frame.
        // pBegin, pEnd, and pUserData will be invalidated when ReleaseOldestFrame() is called.
        FrameMarkers GetOldestFrameMarkers() const
        {
            if (m_unreadFrameCount == 0)
            {
                return FrameMarkers{};
            }

            const Frame& frame = m_pFramesRing[m_readFrameIndex];

            FrameMarkers frameMarkers;
            frameMarkers.validMarkerCount = frame.validMarkerCount;
            frameMarkers.droppedMarkerCount = frame.droppedMarkerCount;
            frameMarkers.droppedNameCharCount = (frame.totalNameCharCount > m_maxNameCharsPerFrame) ? frame.totalNameCharCount - m_maxNameCharsPerFrame : 0;
            frameMarkers.pBegin = m_pMarkersRing + (m_readFrameIndex * m_maxMarkersPerFrame);
            frameMarkers.pEnd = frameMarkers.pBegin + frame.validMarkerCount;
            frameMarkers.pUserData = &frame.userData;
            return frameMarkers;
        }

        // Releases the oldest frame and frees it to be used for writing.
        // This invalidates any pointers retreived with GetOldestFrameMarkers()
        void ReleaseOldestFrame()
        {
            if (!m_unreadFrameCount)
            {
                return; // no frames stored, nothing to do
            }

            // DFD: the frame is cleared at OnFrameEnd() instead of here, so we can still see consumed frames

            m_unreadFrameCount--;
            m_readFrameIndex = CircularIncrement(m_readFrameIndex, m_maxFrames);
        }

        // When called, frameUserDataFn will be called with FrameUserDataFnParams populated with the current frame's data.
        template <class FrameUserDataFn>
        void UpdateCurrentFrameUserData(FrameUserDataFn&& frameUserDataFn)
        {
            Frame& frame = m_pFramesRing[m_writeFrameIndex];

            FrameUserDataFnParams frameUserDataFnParams;
            frameUserDataFnParams.validMarkerCount = frame.validMarkerCount;
            frameUserDataFnParams.droppedMarkerCount = frame.droppedMarkerCount;
            frameUserDataFnParams.droppedCharCount = (frame.totalNameCharCount > m_maxNameCharsPerFrame) ? frame.totalNameCharCount - m_maxNameCharsPerFrame : 0;
            frameUserDataFnParams.pUserData = &(frame.userData);

            frameUserDataFn(frameUserDataFnParams);
        }
    };

}} // namespace nv::perf
