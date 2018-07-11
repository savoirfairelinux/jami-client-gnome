
#pragma once

#include <memory>
#include <thread>
#include <vector>
#include <mutex>
#include <condition_variable>

#include <glib.h>

extern "C" {
#include <darknet.h>
}

#include <iostream>


static void destroyImage(image* img) {
    free_image(*img);
}

static void destroyNet(network* net_) {
std::cout << "yapla    yaapla"<< std::endl ;
    free_network(net_);
}

class NeuralNetwork {
public:
    NeuralNetwork(const char *datacfg, const char *cfgfile, const char *weightfile, float thresh, float hier_thresh);
    ~NeuralNetwork();

    void addImage(guchar* buffertoanalyze, size_t w, size_t h);
    std::shared_ptr<std::vector<guchar>> getResult() { return lastOutput; };
    void stop() {
        running = false;
        inputCv.notify_all();
    }

private:
    using Network = std::unique_ptr<network, decltype(&destroyNet)>;
    Network net_;
    Network initNetwork(const char *datacfg, const char *cfgfile, const char *weightfile);

    std::shared_ptr<std::vector<guchar>> process(image&);

    bool running {true};
    std::thread t;

    std::mutex inputLock;
    std::condition_variable inputCv;
    std::unique_ptr<image, decltype(&destroyImage)> lastInput;

    std::shared_ptr<std::vector<guchar>> lastOutput {nullptr};
    const float thresh;
    const float hier_thresh;
    char **names;
    image **alphabet;
};
