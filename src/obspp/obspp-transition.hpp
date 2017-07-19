#pragma once

#include <vector>

#include "obspp-source.hpp"
#include "obspp-input.hpp"

namespace obs {

class transition : public source {
public:
    transition(std::string &id, std::string &name, obs_data_t *settings = nullptr);
    transition(transition &copy);
    transition(obs_source_t *source);

    void start(int ms, obs::source &source);
    void set(obs::source &source);
    obs::source get_active_source();
    void clear();

    static std::vector<std::string> types();
};

}