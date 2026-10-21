#include "ui/MI_Image.h"
#include "ui/MI_SelectField.h"
#include "ui/MI_SliderField.h"
#include "GameValues.h"
#include "ResourceManager.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
CGameValues game_values;
CResourceManager *rm=nullptr;
short joystickcount=0;
SDL_Joystick **joysticks=nullptr;
SDL_Surface *blitdest=nullptr;
// Rendering is intentionally excluded; selection and input are production code.
void gfxSprite::draw(int,int,const SDL_Rect&) const { std::abort(); }
void gfxSprite::drawStretch(const SDL_Rect&,SDL_Surface*,const SDL_Rect&) const { std::abort(); }
void gfxFont::draw(int,int,std::string_view,int,int) const { std::abort(); }
class Field : public MI_SelectField<short> {
public:
    Field():MI_SelectField(nullptr,0,0,"fixture",400,150){}
    using MI_SelectField::moveNext;
    using MI_SelectField::movePrev;
};
int main()
{
    Field field;short output=-1;
    for (short value:{0,5,10}) field.add(std::to_string(value),value);
    field.setOutputPtr(&output);field.allowWrap(false);assert(field.setCurrentValue(0));
    for(int i=0;i<20;++i) {
        assert(!field.movePrev());assert(field.currentValue()==0 && output==0);
    }
    assert(field.setCurrentValue(10));
    for(int i=0;i<20;++i) {
        assert(!field.moveNext());assert(field.currentValue()==10 && output==10);
    }
    field.hideItem(0,true);field.hideItem(10,true);assert(field.setCurrentValue(5));
    for(int i=0;i<20;++i) {
        assert(!field.moveNext());assert(field.currentValue()==5 && output==5);
        assert(!field.movePrev());assert(field.currentValue()==5 && output==5);
    }
    field.hideItem(0,false);field.hideItem(10,false);
    field.hideItem(5,true);assert(field.setCurrentValue(0));
    assert(field.moveNext() && field.currentValue()==10 && output==10);
    assert(field.movePrev() && field.currentValue()==0 && output==0);
    field.allowWrap(true);
    assert(field.movePrev() && field.currentValue()==10 && output==10);
    assert(field.moveNext() && field.currentValue()==0 && output==0);
    field.hideAllItems(true);
    assert(!field.moveNext() && field.currentValue()==0 && output==0);
    assert(!field.movePrev() && field.currentValue()==0 && output==0);
    Field empty;assert(!empty.moveNext() && !empty.movePrev());
    Field single;single.add("only",7);single.setOutputPtr(&output);single.setCurrentValue(7);
    for(bool wraps:{false,true}) {
        single.allowWrap(wraps);
        assert(!single.moveNext() && single.currentValue()==7 && output==7);
        assert(!single.movePrev() && single.currentValue()==7 && output==7);
    }
    MI_SliderField slider(nullptr,nullptr,0,0,"Extra Time",400,150,320);
    for(short value=0;value<=100;value+=5)slider.add(std::to_string(value),value);
    slider.setOutputPtr(&output);slider.allowWrap(false);slider.setCurrentValue(50);
    CPlayerInput input;
    input.outputControls[0].menu_scrollfast.fPressed=true;
    slider.SendInput(&input);assert(slider.currentValue()==0 && output==0);
    slider.SendInput(&input);assert(slider.currentValue()==100 && output==100);
    slider.SendInput(&input);assert(slider.currentValue()==0 && output==0);
    input.outputControls[0].menu_scrollfast.fPressed=false;
    input.outputControls[0].menu_left.fPressed=true;
    for(int i=0;i<30;++i) {slider.SendInput(&input);assert(slider.currentValue()==0 && output==0);}
    input.outputControls[0].menu_left.fPressed=false;
    input.outputControls[0].menu_right.fPressed=true;
    for(int i=0;i<40;++i)slider.SendInput(&input);
    assert(slider.currentValue()==100 && output==100);
    std::puts("PASS: repeated slider boundaries, hidden items, wrap-enabled selection and production fast-scroll input (rendering excluded)");
}
