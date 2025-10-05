#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <cerrno>
#include <cstring>
#include <cassert>

#include <hybris/hwc2/hwc2_compatibility_layer.h>
#include <hybris/gralloc/gralloc.h>
#include <hybris/platforms/common/windowbuffer.h>

#include <systemd/sd-daemon.h>
#include <linux/kgsl.h>

int main() {
    // --- KGSL setup ---
    int kgsl_fd = open("/dev/kgsl-3d0", O_RDWR);
    if (kgsl_fd < 0) {
        perror("open /dev/kgsl-3d0 failed");
        return -1;
    }

    // --- HWC2 setup ---
    hwc2_compat_device_t* hwcDevice = hwc2_compat_device_new(false);
    assert(hwcDevice);

    HWC2EventListener eventListener = {nullptr, nullptr, nullptr};
    hwc2_compat_device_register_callback(hwcDevice, &eventListener, 0);

    hwc2_compat_display_t* hwcDisplay = nullptr;
    for (int i = 0; i < 5000; ++i) { // wait 5s max
        hwcDisplay = hwc2_compat_device_get_display_by_id(hwcDevice, 0);
        if (hwcDisplay) break;
        usleep(1000);
    }
    assert(hwcDisplay);

    hwc2_compat_display_set_power_mode(hwcDisplay, HWC2_POWER_MODE_ON);
    HWC2DisplayConfig* config = hwc2_compat_display_get_active_config(hwcDisplay);

    int width = config->width;
    int height = config->height;

    // --- Allocate KGSL buffer ---
    struct kgsl_gpumem_alloc alloc{};
    alloc.size = width * height * 4; // RGBA_8888
    alloc.flags = KGSL_MEMFLAGS_GPUREADWRITE;

    if (ioctl(kgsl_fd, IOCTL_KGSL_GPUMEM_ALLOC, &alloc) < 0) {
        perror("kgsl alloc failed");
        close(kgsl_fd);
        return -1;
    }

    void* kgsl_ptr = mmap(0, alloc.size, PROT_READ | PROT_WRITE, MAP_SHARED, kgsl_fd, alloc.gpuaddr);
    if (kgsl_ptr == MAP_FAILED) {
        perror("kgsl mmap failed");
        close(kgsl_fd);
        return -1;
    }

    // --- Create HWC2 layer ---
    hwc2_compat_layer_t* layer = hwc2_compat_display_create_layer(hwcDisplay);
    hwc2_compat_layer_set_composition_type(layer, HWC2_COMPOSITION_CLIENT);
    hwc2_compat_layer_set_blend_mode(layer, HWC2_BLEND_MODE_NONE);
    hwc2_compat_layer_set_source_crop(layer, 0, 0, width, height);
    hwc2_compat_layer_set_display_frame(layer, 0, 0, width, height);
    hwc2_compat_layer_set_visible_region(layer, 0, 0, width, height);

    // --- Notify systemd ---
    sd_notifyf(0, "MAINPID=%lu", (unsigned long)getpid());
    sd_notify(0, "READY=1");
    sd_notify(0, "STATUS=KGSL display ready.");

    // --- Simple rendering loop ---
    while (true) {
        // Contoh: isi buffer dengan warna merah
        std::memset(kgsl_ptr, 0xFF, alloc.size);

        int presentFence;
        hwc2_error_t error = hwc2_compat_display_set_client_target(hwcDisplay, 0, (buffer_handle_t)kgsl_ptr, -1, HAL_DATASPACE_UNKNOWN);
        if (error != HWC2_ERROR_NONE) {
            std::cerr << "Failed to set client target: " << error << std::endl;
            break;
        }
        error = hwc2_compat_display_present(hwcDisplay, &presentFence);
        if (error != HWC2_ERROR_NONE) {
            std::cerr << "Failed to present display: " << error << std::endl;
            break;
        }

        usleep(16 * 1000); // ~60Hz
    }

    munmap(kgsl_ptr, alloc.size);
    close(kgsl_fd);
    return 0;
}
