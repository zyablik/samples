#include <string>
#include <sys/time.h>
class FPS {
public:
    FPS() {
        if(frames == 0) {
            gettimeofday(&start_time, nullptr);
            frames++;
        } else {
            struct timeval current_time;
            gettimeofday(&current_time, nullptr);
            uint32_t delta_msec = ((current_time.tv_sec - start_time.tv_sec) * 1000000 + (current_time.tv_usec - start_time.tv_usec)) / 1000.0;
            if(delta_msec >= 1000) {
                printf("delta_msec = %d frames = %d fps = %.2f\n", delta_msec, frames, frames * 1000.0 / delta_msec);
                frames = 0;
            } else {
                frames++;
            }
        }
    }

    static struct timeval start_time;
    static uint32_t frames;
};

struct timeval FPS::start_time;
uint32_t FPS::frames = 0;

class Duration {
public:
    Duration(const std::string& log): log(log)
    {
        gettimeofday(&start_time, nullptr);
    }

    ~Duration() {
        struct timeval current_time;
        gettimeofday(&current_time, nullptr);
        uint32_t delta_msec = ((current_time.tv_sec - start_time.tv_sec) * 1000000 + (current_time.tv_usec - start_time.tv_usec)) / 1000.0;
        printf("%s duration = %d millisecs\n", log.c_str(), delta_msec);
    }

private:
    std::string log;
    struct timeval start_time;
};