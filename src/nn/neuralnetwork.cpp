#include "neuralnetwork.h"
#include <iostream>


NeuralNetwork::NeuralNetwork(const char *datacfg, const char *cfgfile, const char *weightfile, float thresh, float hier_thresh)
    : net_(nullptr, free_network), lastInput(nullptr, destroyImage), thresh(thresh), hier_thresh(hier_thresh)
{
    t = std::thread([this, datacfg, cfgfile, weightfile] {
        alphabet = load_alphabet();
        net_ = initNetwork(datacfg, cfgfile, weightfile);
        while (running) {
            decltype(lastInput) input(nullptr, nullptr);
            {
                std::unique_lock<std::mutex> l(inputLock);
                inputCv.wait(l, [this]{
                    return not running or lastInput;
                });
                if (not running) {
                    break;
                }
                input = std::move(lastInput);
            }
            lastOutput = process(*input);
        }
    });
}

NeuralNetwork::~NeuralNetwork() {
    stop();
    t.join();
}


NeuralNetwork::Network
NeuralNetwork::initNetwork(const char *datacfg, const char *cfgfile, const char *weightfile) {
    

    list *options = read_data_cfg((char*)datacfg);
    int classNum = std::atoi(option_find_str(options, (char*) "classes", nullptr)); //work around to get correct number for classes
    char *name_list = option_find_str(options, (char*) "names", nullptr);

    names = get_labels(name_list);
    free_list(options);
    
    Network net { load_network((char*)cfgfile, (char*)weightfile, 0), free_network }; //should find classes # here but bugs
    set_batch_network(net.get(), 1);
    net->layers[net->n-1].classes = classNum; // bug fixed here

    return net;
}

void
NeuralNetwork::addImage(guchar* buffertoanalyze, size_t w, size_t h) {
    decltype(lastInput) im { new image(make_image(w, h, 3)), destroyImage };

    for(size_t j = 0; j < im->h ; ++j){
        for(size_t i = 0; i < im->w ; ++i) {
            //direct conversion BGRA->darknet format
            im->data[i + im->w * j ] = buffertoanalyze[(i + im->w * j) * 4 +2]/255.;
            im->data[i + im->w * j + im->w * im->h ] = buffertoanalyze[(i + im->w * j) * 4 + 1]/255.;
            im->data[i + im->w * j + im->w * im->h * 2] = buffertoanalyze[(i + im->w * j) * 4]/255.;
        }
    }                    

    {
        std::lock_guard<std::mutex> l(inputLock);
        lastInput = std::move(im);
    }
    inputCv.notify_all();
}

std::shared_ptr<std::vector<guchar>>
NeuralNetwork::process(image& im)
{
    float nms=0.45; //metric to say if 2 boxes with same label are the same object

    image sized = letterbox_image(im, net_->w, net_->h);
    layer l = net_->layers[net_->n-1];

    auto time=what_time_is_it_now();
    network_predict(net_.get(), sized.data);
    auto dt =  what_time_is_it_now()-time;
    free_image(sized);
    int nboxes = 0;
    detection *dets = get_network_boxes(net_.get(), im.w, im.h, thresh, hier_thresh, 0, 1, &nboxes);
    if (nms){
        do_nms_sort(dets, nboxes, l.classes, nms);
    }

   

    //save_image(im, "plop");
    printf("Predicted in %f seconds, found %d objects (%d classes) thresh:%f hier_thresh:%f.\n", dt, nboxes, l.classes, thresh, hier_thresh);

    image imempty= make_image(im.w, im.h, 3); // creates black picture

    for (size_t i = 0; i < im.w *im.h *3; ++i)
    {
         imempty.data[i]= 1; //converts to white picture
    }

    draw_detections(imempty, dets, nboxes, thresh, names, alphabet, l.classes); // draw colored box black text
    free_detections(dets, nboxes);
    std::shared_ptr<std::vector<guchar>> yoloboxrenorm(new std::vector<guchar>((size_t)(im.w * im.h * 3)));

    // conversion from darknet image to rgb standard  
    for(size_t k = 0; k < 3; ++k){
        for(size_t j = 0; j < im.h; ++j){
            for(size_t i = 0; i < im.w; ++i) {
                size_t dst_index = i + im.w * j + im.w * im.h * k;
                size_t src_index = k + 3*i + 3 * im.w * j;
                (*yoloboxrenorm)[src_index] = imempty.data[dst_index] * 255.f;
            }
        }
    }

    free_image(imempty);
    
    return yoloboxrenorm;
}