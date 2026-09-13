#include "../src/story/DialoguePlayer.h"
#include <iostream>
#include <cstdlib>
#define CHECK(value) do { if(!(value)) { std::cerr << "Failed at " << __LINE__ << ": " << #value << '\n'; std::exit(1); } } while(false)

int main() {
    story::DialoguePlayer dialogue;
    CHECK(dialogue.Start("tools/fixtures/dialogue_expression.dialogue"));
    dialogue.Update(1,false); // portrait fade-in
    const auto next=[&] {dialogue.Update(10,false);dialogue.Update(0,true);};
    CHECK(dialogue.Expression()=="yan");
    next();CHECK(dialogue.Expression()=="yan"); // protagonist: blackout?
    next();CHECK(dialogue.Expression()=="yan"); // protagonist again
    next();CHECK(dialogue.Expression()=="ase");
    next();CHECK(dialogue.Expression()=="ase"); // player has the flashlight
    next();CHECK(dialogue.Expression()=="smile");
    dialogue.Update(10,false); // whispered line auto-advances
    CHECK(dialogue.Expression()=="smile"); // protagonist keeps smile
    next();CHECK(dialogue.Expression()=="normal");
    next();CHECK(dialogue.Expression()=="nihi");
    next();CHECK(dialogue.Expression()=="nihi");
    CHECK(dialogue.Start("asset/story/beach_arrival.dialogue"));
    CHECK(dialogue.Expression()=="hide"); // a new part must not inherit nihi
    dialogue.Reset();CHECK(dialogue.Expression()=="normal");
    std::cout<<"Dialogue expression retention passed\n";
}
