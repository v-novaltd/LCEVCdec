/* Copyright (c) V-Nova International Limited 2023. All rights reserved. */

#include "buffer_manager.h"

#include "uLog.h"

#include <LCEVC/lcevc_dec.h>

#include <algorithm>
#include <vector>

// ------------------------------------------------------------------------------------------------

using namespace lcevc_dec::api_utility;

namespace lcevc_dec::decoder {

// - BufferManager ----------------------------------------------------------------------------------

void BufferManager::release()
{
    m_buffersBusy.clear();
    m_buffersFree.clear();
}

PictureBuffer* BufferManager::getBuffer(size_t requiredSize)
{
    // Grab from free set
    if (!m_buffersFree.empty()) {
        std::shared_ptr<PictureBuffer> bufOut = (*m_buffersFree.begin());
        m_buffersBusy.insert(bufOut);
        m_buffersFree.erase(bufOut);
        bufOut->clear();
        if (bufOut->size() < requiredSize) {
            bufOut->resize(requiredSize);
        }
        return bufOut.get();
    }

    // Create a new one and mark it as busy
    std::pair<BufSet::iterator, bool> result =
        m_buffersBusy.emplace(std::make_shared<PictureBuffer>(requiredSize));
    if (result.second == false) {
        VNLogError("Could not create buffer\n");
        return nullptr;
    }
    return result.first->get();
}

bool BufferManager::releaseBuffer(PictureBuffer* buffer)
{
    auto iterToBusyBuf = m_buffersBusy.find(buffer);

    if (iterToBusyBuf == m_buffersBusy.end()) {
        if (m_buffersFree.count(buffer) == 0) {
            VNLogWarning("Freeing buffer but it was already free.\n");
        } else {
            VNLogError("Freeing buffer, but it doesn't appear to exist anywhere!\n");
        }
        return false;
    }

    m_buffersFree.insert(*iterToBusyBuf);
    m_buffersBusy.erase(*iterToBusyBuf);
    return true;
}

} // namespace lcevc_dec::decoder
