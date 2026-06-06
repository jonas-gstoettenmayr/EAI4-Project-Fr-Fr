#include <iostream>
#include <memory>
#include <vector>
#include <fstream>
#include <sys/mman.h>
#include <unistd.h>
#include <libcamera/libcamera.h>
#include "lodepng.h" // Drop-in PNG encoder

using namespace libcamera;

int main() {
    // 1. Initialize the Camera Manager
    std::unique_ptr<CameraManager> cm = std::make_unique<CameraManager>();
    if (cm->start()) {
        std::cerr << "Failed to start camera manager" << std::endl;
        return EXIT_FAILURE;
    }

    if (cm->cameras().empty()) {
        std::cerr << "No cameras found" << std::endl;
        return EXIT_FAILURE;
    }

    // 2. Acquire the camera
    std::shared_ptr<Camera> camera = cm->cameras()[0];
    camera->acquire();

    // 3. Configure for Still Capture in RGB
    std::unique_ptr<CameraConfiguration> config = 
        camera->generateConfiguration({ StreamRole::StillCapture });
    
    StreamConfiguration &streamConfig = config->at(0);
    streamConfig.pixelFormat = formats::RGB888; 
    streamConfig.size.width = 1280;  // Resolution width
    streamConfig.size.height = 720;  // Resolution height

    config->validate();
    camera->configure(config.get());

    // 4. Allocate Memory Buffer for the stream
    FrameBufferAllocator *allocator = new FrameBufferAllocator(camera);
    Stream *stream = streamConfig.stream();
    allocator->allocate(stream);

    // 5. Build and Queue a Capture Request
    camera->start();
    std::unique_ptr<Request> request = camera->createRequest();
    const std::vector<std::unique_ptr<FrameBuffer>> &buffers = allocator->buffers(stream);
    request->addBuffer(stream, buffers[0].get());
    camera->queueRequest(request.get());

    std::cout << "Capturing frame..." << std::endl;

    // 6. Wait for the frame (In production, use a proper Request complete callback)
    // For a quick single-shot example, we can poll or sleep briefly for the ISP to finish.
    std::this_thread::sleep_for(std::chrono::milliseconds(500)); 

    // 7. Map the DMA buffer into CPU memory space to read the raw RGB data
    FrameBuffer *buffer = buffers[0].get();
    const FrameBuffer::Plane &plane = buffer->planes()[0];
    
    // libcamera uses file descriptors (fd) for cross-process DMA buffers.
    // We use mmap to access the memory page directly.
    int fd = plane.fd.get();
    size_t length = plane.length;
    void *mem = mmap(NULL, length, PROT_READ, MAP_SHARED, fd, 0);

    if (mem == MAP_FAILED) {
        std::cerr << "Failed to mmap buffer memory" << std::endl;
        return EXIT_FAILURE;
    }

    // 8. Convert the raw memory pointer to a PNG via Lodepng
    unsigned char* rgbData = static_cast<unsigned char*>(mem);
    unsigned width = streamConfig.size.width;
    unsigned height = streamConfig.size.height;

    std::cout << "Encoding to PNG..." << std::endl;
    unsigned error = lodepng::encode("snapshot.png", rgbData, width, height, LCT_RGB, 8);

    if (error) {
        std::cerr << "PNG encoder error " << error << ": " << lodepng_error_text(error) << std::endl;
    } else {
        std::cout << "Success! Saved snapshot.png (" << width << "x" << height << ")" << std::endl;
    }

    // 9. Clean up memory mappings and camera hardware
    munmap(mem, length);
    camera->stop();
    allocator->free(stream);
    delete allocator;
    camera->release();
    cm->stop();

    return EXIT_SUCCESS;
}