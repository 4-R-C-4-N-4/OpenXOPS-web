"""Exercise the real human::PickupWeapon body with minimal weapon/motion doubles.

The original game picks up into the selected EMPTY slot; it does not silently
replace the selected occupied slot. This guards that contract without linking
rendering/audio or duplicating the function's implementation in Python.
"""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


@unittest.skipUnless(shutil.which('g++'), 'g++ required for production-body test')
class WeaponPickupTests(unittest.TestCase):
    def test_selected_empty_slot_accepts_second_weapon_and_preserves_first(self):
        source = (ROOT / 'src/object.cpp').read_text()
        start = source.index('int human::PickupWeapon(')
        opening = source.index('{', start)
        depth = 1
        end = opening + 1
        while depth:
            depth += (source[end] == '{') - (source[end] == '}')
            end += 1
        production_body = source[start:end]
        doubles = r'''
#include <cassert>
#include <cstddef>
class weapon {
public:
    int id; bool used=false;
    explicit weapon(int value):id(value){}
    int Pickup(){ if(used)return 1;used=true;return 0; }
    void GetParamData(int* out,int*,int*){if(out)*out=id;}
};
struct Motion { int last=-1;void PickupWeapon(int id){last=id;} };
class human {
public:
    class weapon* weapon[2]{};
    int selectweapon=0,noweapon_bullets[2]{},noweapon_loadbullets[2]{},selectweaponcnt=0;
    Motion motion;Motion* MotionCtrl=&motion;
    int PickupWeapon(class weapon* in_weapon);
};
'''
        checks = r'''
int main(){
    human p;weapon initial(3),incoming(9),third(6);
    p.weapon[0]=&initial;
    // An occupied selected slot rejects pickup, rather than overwriting it.
    assert(p.PickupWeapon(&incoming)==0 && !incoming.used);
    // Selecting slot 2 (index 1) is the game's normal pickup operation.
    p.selectweapon=1;
    assert(p.PickupWeapon(&incoming)==1);
    assert(p.weapon[0]==&initial && p.weapon[1]==&incoming);
    assert(p.motion.last==9 && p.selectweaponcnt==10);
    assert(p.PickupWeapon(&third)==0 && !third.used);
    assert(p.PickupWeapon(nullptr)==0);
}
'''
        with tempfile.TemporaryDirectory(prefix='weapon_pickup_') as temp:
            fixture = Path(temp) / 'pickup.cpp'
            binary = Path(temp) / 'pickup'
            fixture.write_text(doubles + production_body + checks)
            result = subprocess.run(['g++','-std=c++17',str(fixture),'-o',str(binary)],
                                    capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stderr)
            result = subprocess.run([str(binary)],capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
