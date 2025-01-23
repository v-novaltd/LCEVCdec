/* Copyright (c) V-Nova International Limited 2023-2024. All rights reserved.
 * This software is licensed under the BSD-3-Clause-Clear License by V-Nova Limited.
 * No patent licenses are granted under this license. For enquiries about patent licenses,
 * please contact legal@v-nova.com.
 * The LCEVCdec software is a stand-alone project and is NOT A CONTRIBUTION to any other project.
 * If the software is incorporated into another project, THE TERMS OF THE BSD-3-CLAUSE-CLEAR LICENSE
 * AND THE ADDITIONAL LICENSING INFORMATION CONTAINED IN THIS FILE MUST BE MAINTAINED, AND THE
 * SOFTWARE DOES NOT AND MUST NOT ADOPT THE LICENSE OF THE INCORPORATING PROJECT. However, the
 * software may be incorporated into a project under a compatible license provided the requirements
 * of the BSD-3-Clause-Clear license are respected, and V-Nova Limited remains
 * licensor of the software ONLY UNDER the BSD-3-Clause-Clear license (not the compatible license).
 * ANY ONWARD DISTRIBUTION, WHETHER STAND-ALONE OR AS PART OF ANY OTHER PROJECT, REMAINS SUBJECT TO
 * THE EXCLUSION OF PATENT LICENSES PROVISION OF THE BSD-3-CLAUSE-CLEAR LICENSE. */

#include "buffer_manager.h"

#include "log.h"

#include <LCEVC/lcevc_dec.h>

#include <cstddef>
#include <memory>
#include <vector>

// ------------------------------------------------------------------------------------------------

namespace lcevc_dec::decoder {

static const LogComponent kComp = LogComponent::BufferManager;

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
