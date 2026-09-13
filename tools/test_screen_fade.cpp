#include "../src/rendering/ScreenFade.h"
#include <cassert>
#include <iostream>

int main() {
    rendering::ScreenFade fade;
    int changes=0;
    for(int i=0;i<40;++i)fade.Update(1.f/60,[&]{++changes;});
    assert(!fade.Active()&&fade.Alpha()==0&&changes==0); // startup fade-in
    assert(fade.BeginOut());
    for(int i=0;i<20;++i)fade.Update(1.f/60,[&]{++changes;});
    assert(changes==0&&fade.CurrentPhase()==rendering::ScreenFade::Phase::Out);
    for(int i=0;i<10;++i)fade.Update(1.f/60,[&]{++changes;});
    assert(changes==1&&fade.CurrentPhase()==rendering::ScreenFade::Phase::In);
    for(int i=0;i<50;++i)fade.Update(1.f/60,[&]{++changes;});
    assert(!fade.Active()&&fade.Alpha()==0&&changes==1);
    std::cout<<"Screen fade transition checks passed\n";
}
