// Local-only browser diagnostics. Not included unless OPENXOPS_TEST_HOOKS=1.
#include "../../src/gamemain.h"
#include <emscripten.h>
#include <GL/gl.h>
extern D3DGraphics d3dg;
extern ObjectManager ObjMgr;
extern ResourceManager Resource;
// Draw one actual engine-composed human, with live pose and game transforms.
// A local visual-inspection helper only; normal builds never compile this file.
extern "C" EMSCRIPTEN_KEEPALIVE void qa_resume_game() {
    emscripten_resume_main_loop();
}
extern "C" EMSCRIPTEN_KEEPALIVE int qa_pose_preview(int id, int view) {
    human* p=ObjMgr.GetHumanObject(id);
    if(!p || !p->GetEnableFlag())return 0;
    emscripten_pause_main_loop();
    float x,y,z,rx;p->GetPosData(&x,&y,&z,&rx);
    const float angle=view * (float)M_PI / 2;
    d3dg.StartRender();
    d3dg.SetFog(false,0);
    glClearColor(.18f,.20f,.23f,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    d3dg.SetCamera(x+sinf(angle)*36,y+11,z+cosf(angle)*36,
                  -(float)M_PI/2-angle,0,VIEWANGLE_NORMAL);
    p->Render(&d3dg,&Resource,false,false,false);
    d3dg.EndRender();
    return 1;
}
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
    int wid=-1, slot0=-1, slot1=-1;
    if(weapons[0])weapons[0]->GetParamData(&slot0,nullptr,nullptr);
    if(weapons[1])weapons[1]->GetParamData(&slot1,nullptr,nullptr);
    if(selected>=0 && selected<TOTAL_HAVEWEAPON && weapons[selected])weapons[selected]->GetParamData(&wid,nullptr,nullptr);
    EM_ASM({Module.qaPlayer=({x:$0,y:$1,z:$2,hp:$3,dead:!!$4,type:$5,team:$6,selected:$7,weapon:$8,slot0:$9,slot1:$10});},x,y,z,p->GetHP(),p->GetDeadFlag(),type,team,selected,wid,slot0,slot1);
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
