#pragma once

class mj_worker {
public:
    virtual void start() = 0;
    virtual void stop() = 0;
};
