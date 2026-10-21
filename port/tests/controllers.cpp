#include "GameValues.h"
#include <cassert>
#include <cstdio>
CGameValues game_values;
short joystickcount = 4;
SDL_Joystick *handles[4]{};
SDL_Joystick **joysticks = handles;
static void button(int device, Uint8 button, bool down, int state)
{
    SDL_Event event{};
    event.type = down ? SDL_JOYBUTTONDOWN : SDL_JOYBUTTONUP;
    event.jbutton.which = SDL_JoystickInstanceID(handles[device]);
    event.jbutton.button = button;
    game_values.playerInput.Update(event, state);
}
int main()
{
    assert(SDL_Init(SDL_INIT_JOYSTICK) == 0);
    for (int i = 0; i < 4; ++i) {
        int index = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER, 4, 8, 1);
        assert(index >= 0);
        handles[i] = SDL_JoystickOpen(index);
        assert(handles[i]);
        SDL_JoystickSetPlayerIndex(handles[i], i + 1); // nxdk uses ports 1..4.
        for (int profile = 0; profile < 2; ++profile) {
            auto &input = game_values.inputConfiguration[i][profile];
            input.iDevice = profile ? i : -1;
            for (auto &group : input.inputGameControls)
                for (auto &key : group.keys) key = profile ? 29 : SDLK_F12;
        }
        auto &input = game_values.inputConfiguration[i][1];
        input.inputGameControls[0].game_jump = JOY_BUTTON_START + i;
        input.inputGameControls[1].menu_select = JOY_BUTTON_START + i;
        game_values.playerInput.inputControls[i] = &input;
        game_values.playercontrol[i] = 1;
    }
    for (int i = 0; i < 4; ++i) assert(nxdk_joystick_slot(SDL_JoystickInstanceID(handles[i])) == i);
    assert(nxdk_joystick_slot(0x7fffffff) == -1);
    auto &input = game_values.playerInput;
    // Real instance-ID translation and simultaneous held inputs on four virtual
    // devices exercise production input code; this is not physical-port proof.
    for (int i = 0; i < 4; ++i) button(i, i, true, 0);
    for (int i = 0; i < 4; ++i) assert(input.outputControls[i].game_jump.fDown);
    for (int i = 0; i < 4; ++i) button(i, i, false, 0);
    for (int i = 0; i < 4; ++i) assert(!input.outputControls[i].game_jump.fDown);
    input.ResetKeys();
    // Missing saved device: available pad's A navigates menu, without changing
    // custom device/binding settings or routing its input into missing gameplay.
    auto &profile = game_values.inputConfiguration[0][1];
    profile.iDevice = 3;
    auto *saved = handles[3]; handles[3] = nullptr;
    assert(nxdk_joystick_slot(SDL_JoystickInstanceID(saved)) == -1);
    button(0, 0, true, 1);
    assert(input.outputControls[0].menu_select.fDown && profile.iDevice == 3);
    button(0, 0, false, 1);
    assert(!input.outputControls[0].menu_select.fDown);
    button(0, 0, true, 0);
    assert(!input.outputControls[0].game_jump.fDown);
    handles[3] = saved;
    input.ResetKeys();
    // Keyboard remains usable while attached pad offers menu recovery.
    input.inputControls[0] = &game_values.inputConfiguration[0][0];
    game_values.inputConfiguration[0][0].inputGameControls[1].menu_cancel = SDLK_ESCAPE;
    button(0, 0, true, 1);
    assert(input.outputControls[0].menu_select.fDown);
    button(0, 0, false, 1);
    SDL_Event key{}; key.type = SDL_KEYDOWN; key.key.keysym.sym = SDLK_ESCAPE;
    input.Update(key, 1);
    assert(input.outputControls[0].menu_cancel.fDown);
    input.ResetKeys();
    button(0, 0, true, 0);
    assert(!input.outputControls[0].game_jump.fDown);
    // A reappearing selected device restores custom bindings and clears the
    // recovery pad's held state. Unknown SDL instance IDs must be ignored.
    input.inputControls[0] = &profile;
    handles[3] = nullptr;
    button(0, 0, true, 1);
    assert(input.outputControls[0].menu_select.fDown);
    handles[3] = saved;
    button(0, 0, true, 1);
    assert(!input.outputControls[0].menu_select.fDown);
    button(3, 0, true, 1);
    assert(input.outputControls[0].menu_select.fDown);
    input.ResetKeys();
    SDL_Event unknown{}; unknown.type = SDL_JOYBUTTONDOWN;
    unknown.jbutton.which = 0x7fffffff; unknown.jbutton.button = 0;
    input.Update(unknown, 1);
    assert(!input.outputControls[0].menu_select.fDown);
    SDL_Joystick *physical[4] = {handles[0], handles[1], handles[2], handles[3]};
    // Dense enumeration reordering must not change physical assignments.
    handles[0] = physical[3]; handles[1] = physical[1];
    handles[2] = physical[0]; handles[3] = physical[2];
    for (int slot = 0; slot < 4; ++slot) {
        assert(nxdk_joystick_for_slot(slot) == physical[slot]);
        assert(nxdk_joystick_slot(SDL_JoystickInstanceID(physical[slot])) == slot);
    }
    input.ResetKeys();
    for (int slot = 0; slot < 4; ++slot) {
        input.inputControls[slot] = &game_values.inputConfiguration[slot][1];
        game_values.inputConfiguration[slot][1].iDevice = slot;
        SDL_Event event{}; event.type = SDL_JOYBUTTONDOWN;
        event.jbutton.which = SDL_JoystickInstanceID(physical[slot]);
        event.jbutton.button = slot;
        input.Update(event, 0);
    }
    for (int slot = 0; slot < 4; ++slot) assert(input.outputControls[slot].game_jump.fDown);
    // Removing physical port one compacts the SDL list; ports 2..4 must retain
    // slots 1..3, rather than becoming players 1..3. Reconnect may append it.
    input.ResetKeys();
    handles[0] = physical[3]; handles[1] = physical[1]; handles[2] = physical[2];
    joystickcount = 3;
    assert(!nxdk_joystick_for_slot(0));
    assert(nxdk_joystick_for_slot(1) == physical[1]);
    assert(nxdk_joystick_for_slot(2) == physical[2]);
    assert(nxdk_joystick_for_slot(3) == physical[3]);
    SDL_Event recover{}; recover.type = SDL_JOYBUTTONDOWN;
    recover.jbutton.which = SDL_JoystickInstanceID(physical[1]); recover.jbutton.button = 0;
    input.Update(recover, 1);
    assert(input.outputControls[0].menu_select.fDown);
    input.ResetKeys();
    recover.jbutton.button = 1;
    input.Update(recover, 0);
    assert(!input.outputControls[0].game_jump.fDown && input.outputControls[1].game_jump.fDown);
    handles[3] = physical[0]; joystickcount = 4;
    input.ResetKeys();
    recover.jbutton.which = SDL_JoystickInstanceID(physical[0]); recover.jbutton.button = 0;
    input.Update(recover, 0);
    assert(input.outputControls[0].game_jump.fDown);
    // Restore enumeration for teardown; every opened handle is closed once.
    for (int slot = 0; slot < 4; ++slot) handles[slot] = physical[slot];
    for (auto *handle : handles) SDL_JoystickClose(handle);
    SDL_Quit();
    puts("PASS: four virtual controllers, physical-slot routing through reordering/removal/reconnect, menu recovery and assignments");
}
