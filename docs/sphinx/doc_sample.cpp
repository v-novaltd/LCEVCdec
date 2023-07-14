#include <LCEVC/lcevc_dec.h>
#include <vector>
#include <deque>

static void eventCallback( LCEVC_DecoderHandle decHandle,
                           LCEVC_Event event,
                           LCEVC_PictureHandle picHandle,
                           const LCEVC_DecodeInformation* decodeInformation,
                           const uint8_t* data, uint32_t dataSize,
                           void* userData );


// Base Start
struct Base {
    std::vector<uint8_t> enhancement;
    int64_t enhancementTimestamp = 0;

    LCEVC_PictureHandle picture = {0};
    int64_t pictureTimestamp = 0;
};

static bool updateBase(LCEVC_DecoderHandle decoder, Base &base, std::deque<LCEVC_PictureHandle> &pool);
// Base End

void fillPool(LCEVC_DecoderHandle decoder, LCEVC_PictureDesc &desc, std::deque<LCEVC_PictureHandle> &pool, uint32_t count);
void writeOutput(LCEVC_PictureHandle picture, int64_t timestamp);

int main(int argc, char **argv)
{
    const int width = 3840;
    const int height = 2160;
    const int maxBasePictures = 4;
    const int maxEnhancedPictures = 4;

    // Creation Start
    LCEVC_DecoderHandle decoderHandle = {};

    if(LCEVC_CreateDecoder(&decoderHandle, LCEVC_AccelContextHandle{} ) != LCEVC_Success) {
    	return 0;
    }

    LCEVC_ConfigureDecoderInt(decoderHandle, "max_width", width);
    LCEVC_ConfigureDecoderInt(decoderHandle, "max_height", height);

    int32_t events[] = {LCEVC_Log, LCEVC_Exit, LCEVC_CanSendBase};
    LCEVC_ConfigureDecoderIntArray(decoderHandle, "events", 3, events);

    LCEVC_SetDecoderEventCallback(decoderHandle, eventCallback, nullptr);

    if(LCEVC_InitializeDecoder(decoderHandle) != 0) {
        return 0;
    }
    // Creation End

    // Decoding Start
    struct Base base;
    std::deque<LCEVC_PictureHandle> basePool;
    std::deque<LCEVC_PictureHandle> enhancedPool;

    LCEVC_PictureDesc defaultDesc = {0};
    LCEVC_DefaultPictureDesc(&defaultDesc, LCEVC_YUV420_RASTER_8, 1920, 1080);
    fillPool(decoderHandle, defaultDesc, basePool, maxBasePictures);
    fillPool(decoderHandle, defaultDesc, enhancedPool, maxEnhancedPictures);

    // Work from back from end end of decode chain from trying to move data along
    do {
        // Receive any pending output pictures
        {
            LCEVC_PictureHandle enhanced;
            LCEVC_DecodeInformation decodeInformation = {0};
            while(LCEVC_ReceiveDecoderPicture(decoderHandle, &enhanced, &decodeInformation) == LCEVC_Success) {
                writeOutput(enhanced, decodeInformation.timestamp);
                enhancedPool.push_back(enhanced);
            }
        }

        // Receive any completed base pictures
        {
            LCEVC_PictureHandle base;
            while(LCEVC_ReceiveDecoderBase(decoderHandle, &base) == LCEVC_Success) {
                basePool.push_back(base);
            }
        }

        // Send enhancement pictures ready to use
        while(!enhancedPool.empty()) {
            if(LCEVC_SendDecoderPicture(decoderHandle, enhancedPool.front()) != LCEVC_Success) {
                break;
            }
            enhancedPool.pop_front();
        }

        // Try to send any enhancement data
        if(!base.enhancement.empty()) {
            if(LCEVC_SendDecoderEnhancementData(decoderHandle, base.enhancementTimestamp, false, base.enhancement.data(), base.enhancement.size()) == LCEVC_Success) {
                base.enhancement.clear();
            }
        }

        // Try to send any base picture
        if(!base.picture.hdl) {
            if(LCEVC_SendDecoderBase(decoderHandle, base.pictureTimestamp, false, base.picture, 1000000, nullptr) == LCEVC_Success) {
                base.picture.hdl = 0;
            }
        }

    // Update from base decoder
    } while(updateBase(decoderHandle, base, basePool));
    // Decoding End

    // Destruction Start
    // Release decoder
    LCEVC_DestroyDecoder(decoderHandle);
    // Destruction End
}

// Update the base docoder state - new base picturews are pulled from given pool
// Rerurn true if decoding should continue
//
static bool updateBase(LCEVC_DecoderHandle decoder, Base &base, std::deque<LCEVC_PictureHandle> &pool)
{
    if(pool.empty())
        return true;

    return true;
}

// Allocatge 'count' pictures and push them into the given pool
void fillPool(LCEVC_DecoderHandle decoder, LCEVC_PictureDesc &desc, std::deque<LCEVC_PictureHandle> &pool, uint32_t count)
{
    for(uint32_t i = 0; i < count; ++i) {
        LCEVC_PictureHandle handle = {0};
        LCEVC_AllocPicture(decoder, &desc, &handle);
        pool.push_back(handle);
    }
}

static void eventCallback( LCEVC_DecoderHandle decHandle,
                           LCEVC_Event event,
                           LCEVC_PictureHandle picHandle,
                           const LCEVC_DecodeInformation* decodeInformation,
                           const uint8_t* data, uint32_t dataSize,
                           void* userData )
{
}
