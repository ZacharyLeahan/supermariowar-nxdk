#include "util/Filesystem.h"
#include "util/CheckedCast.h"
#include <cassert>
#include <iostream>
struct Root { SMW_RTTI_ROOT(Root) public: virtual ~Root() = default; };
struct Moving: Root { SMW_RTTI(Moving,Root) };
struct Shell: Moving { SMW_RTTI(Shell,Moving) };
struct Block: Root { SMW_RTTI(Block,Root) };
int main() {
    Shell shell; Block block;
    Root* p=&shell; Root* q=&block;
    assert(smw::checked_cast<Shell*>(p)==&shell);
    assert(smw::checked_cast<Moving*>(p)==static_cast<Moving*>(&shell));
    assert(smw::checked_cast<Block*>(p)==nullptr);
    assert(smw::checked_cast<Shell*>(q)==nullptr);
    assert(smw::checked_cast<Shell*>(static_cast<Root*>(nullptr))==nullptr);
    using smw::filesystem::path;
    assert((path("D:/data/")/"maps/0smw.map").string()=="D:\\data\\maps\\0smw.map");
    assert(path("D:/data/maps/test.map").filename().string()=="test.map");
    assert(path("D:/data/maps/test.map").extension().string()==".map");
    assert(path("README").extension().empty());
    assert((path()/"Music.txt").string()=="Music.txt");
    std::cout << "PASS: checked casts and Xbox path operations (host execution only)\n";
}
