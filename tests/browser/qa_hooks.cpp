// Local-only browser diagnostics. Not included unless OPENXOPS_TEST_HOOKS=1.
#include "../../src/gamemain.h"
#include <emscripten.h>
#include <GL/gl.h>
extern D3DGraphics d3dg;
extern ObjectManager ObjMgr;
extern ResourceManager Resource;
extern "C" EMSCRIPTEN_KEEPALIVE int qa_weapon_resources() {
    int valid=0;
    for(int i=1;i<TOTAL_PARAMETERINFO_WEAPON;i++) {
        int model=-1, texture=-1;
        if(Resource.GetWeaponModelTexture(i,&model,&texture)==0 && model>=0 && texture>=0)valid++;
    }
    return valid;
}
extern "C" EMSCRIPTEN_KEEPALIVE void qa_player_state() {
    human* p=ObjMgr.GetHumanObject(ObjMgr.GetPlayerID());
    float x,y,z,rx;p->GetPosData(&x,&y,&z,&rx);
    int type=-1,data=-1,team=-1,selected=-1;short tag=0;
    p->GetParamData(&type,&data,&tag,&team);
    weapon* weapons[TOTAL_HAVEWEAPON];int ln[TOTAL_HAVEWEAPON],n[TOTAL_HAVEWEAPON];
    p->GetWeapon(&selected,weapons,ln,n);
    int wid=-1;
    if(selected>=0 && selected<TOTAL_HAVEWEAPON && weapons[selected])weapons[selected]->GetParamData(&wid,nullptr,nullptr);
    EM_ASM({Module.qaPlayer=({x:$0,y:$1,z:$2,hp:$3,dead:!!$4,type:$5,team:$6,selected:$7,weapon:$8});},x,y,z,p->GetHP(),p->GetDeadFlag(),type,team,selected,wid);
}
extern "C" EMSCRIPTEN_KEEPALIVE int qa_give_weapon(int id) {
    return ObjMgr.CheatNewWeapon(ObjMgr.GetPlayerID(), id) ? 1 : 0;
}
extern "C" EMSCRIPTEN_KEEPALIVE int qa_reticle_pixels(int fog) {
    d3dg.StartRender();
    d3dg.SetFog(fog != 0, 1);
    glClearColor(1,1,1,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    const int black=d3dg.GetColorCode(0,0,0,1);
    d3dg.Draw2DLine(140,240,500,240,black);
    d3dg.Draw2DLine(320,60,320,420,black);
    d3dg.Draw2DBox(140,239,500,241,d3dg.GetColorCode(0,0,0,.5f));
    d3dg.Draw2DBox(319,60,321,420,d3dg.GetColorCode(0,0,0,.5f));
    unsigned char row[360*4];
    glReadPixels(140,239,360,1,GL_RGBA,GL_UNSIGNED_BYTE,row);
    int dark=0;
    for(int i=0;i<360;i++)if(row[i*4]<200&&row[i*4+1]<200&&row[i*4+2]<200)dark++;
    return dark;
}
