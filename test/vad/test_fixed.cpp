#include "audio_detector.h"
#include <cassert>
#include <algorithm>
#include <cstdio>
static int16_t frame[320], window[16000];
static void feed(int value,int count){std::fill(frame,frame+320,value);while(count--)audioDetectorProcess(frame,320,false,window,16000);}
int main(){
    audioDetectorBegin();
    auto s=audioDetectorGetStats();assert(s.calibrated && s.highThreshold==1000 && s.lowThreshold==800);
    feed(300,100);assert(audioDetectorGetStats().events==0);
    feed(1000,10);feed(300,20);assert(audioDetectorGetStats().events==0);
    feed(1500,2);feed(300,20);assert(audioDetectorGetStats().events==0);
    feed(1500,3);feed(900,10);assert(audioDetectorGetStats().events==0);
    feed(300,12);assert(audioDetectorGetStats().events==0);
    feed(300,1);assert(audioDetectorGetStats().events==1);
    feed(300,40);assert(audioDetectorGetStats().windowsReady==1);
    audioDetectorBegin();feed(30000,25);feed(300,60);
    s=audioDetectorGetStats();assert(s.highThreshold==1000 && s.lowThreshold==800);
    audioDetectorResetStream();s=audioDetectorGetStats();assert(s.calibrated && s.highThreshold==1000 && s.lowThreshold==800);
    audioDetectorBegin();feed(300,50);feed(1500,250);
    assert(audioDetectorGetStats().windowsReady>=5);
    feed(300,30);s=audioDetectorGetStats();
    assert(s.events==1 && s.skipped==0 && s.windowsReady>=6);
    // A short event near the limit retains one centered window, even with
    // its 260 ms quiet tail; quiet frames must not trigger rolling mode.
    audioDetectorBegin();feed(300,50);feed(1500,40);feed(300,40);
    s=audioDetectorGetStats();assert(s.events==1 && s.windowsReady==1 && s.skipped==0);
    // Reset midway through a long event must not leak its history into silence.
    audioDetectorBegin();feed(1500,80);audioDetectorResetStream();
    const auto before=audioDetectorGetStats().windowsReady;
    feed(300,80);assert(audioDetectorGetStats().windowsReady==before);
    // Short impulse meeting onset (60 ms) but total activity < 150 ms (80 ms) must be skipped.
    audioDetectorBegin();feed(300,50);
    feed(1500,3);feed(900,1);feed(300,13);
    s=audioDetectorGetStats();
    assert(s.events==1 && s.skipped==1 && s.windowsReady==0);
    puts("PASS: fixed VAD silence, onset, hysteresis, quiet duration, startup spike, min duration and reset");
}
