// Local-only browser diagnostics. Not included unless OPENXOPS_TEST_HOOKS=1.
#include "../../src/gamemain.h"
#include <emscripten.h>
#include <GL/gl.h>
extern D3DGraphics d3dg;
extern ObjectManager ObjMgr;
extern ResourceManager Resource;
// Draw one actual engine-composed human, with live pose and game transforms.
// A local visual-inspection helper only; normal builds never compile this file.
// Spawn the engine's own blood + impact-smoke effects a few units in front of
// the player so effect textures can be inspected without a firefight.
extern "C" EMSCRIPTEN_KEEPALIVE int qa_spawn_effects(float x, float y, float z) {
    // Same textures/alpha as SetHumanBlood (objectmanager.cpp:638), the map-hit
    // impact puff (:1032) and the muzzle flash (:2118); long-lived and larger.
    ObjMgr.AddEffect(x,y,z,0,0,0,0,0,0,4.0f,0,Resource.GetEffectBloodTexture(),10,0,1.0f,0,EFFECT_NORMAL,600);
    ObjMgr.AddEffect(x,y-5,z,0,0,0,0,0,0,4.0f,0,Resource.GetEffectSmokeTexture(),10,0,0.625f,0,EFFECT_NORMAL,600);
    ObjMgr.AddEffect(x,y+5,z,0,0,0,0,0,0,4.0f,0,Resource.GetEffectMflashTexture(),0,0,0.63f,0,EFFECT_NORMAL,600);
    return 1;
}
// Kill every living hostile currently in the world (wave QA). Returns the
// number killed; staged/remaining counts are exported on Module.qaWave.
extern "C" EMSCRIPTEN_KEEPALIVE int qa_kill_hostiles() {
    human* me=ObjMgr.GetPlayerHumanObject();
    int myteam=0,team=0,n=0;
    me->GetParamData(NULL,NULL,NULL,&myteam);
    for(int i=0;i<MAX_HUMAN;i++){
        human* h=ObjMgr.GetHumanObject(i);
        if(!h||!h->GetEnableFlag()||h->GetHP()<=0)continue;
        h->GetParamData(NULL,NULL,NULL,&team);
        if(team!=myteam){h->SetHP(0);n++;}
    }
    return n;
}
extern "C" EMSCRIPTEN_KEEPALIVE void qa_test_health() {
    ObjMgr.GetPlayerHumanObject()->SetHP(100000);
}
extern "C" EMSCRIPTEN_KEEPALIVE int qa_teleport_to_human(int id) {
    human* h=ObjMgr.GetHumanObject(id);
    if(!h || !h->GetEnableFlag())return 0;
    float x,y,z,rx;h->GetPosData(&x,&y,&z,&rx);
    ObjMgr.GetPlayerHumanObject()->SetPosData(x,y,z,rx);
    return 1;
}
extern "C" EMSCRIPTEN_KEEPALIVE int qa_teleport_to_drop(int type) {
    for(int i=0;i<MAX_WEAPON;i++){
        weapon* w=ObjMgr.GetWeaponObject(i);
        int id=-1;if(!w || !w->GetEnableFlag() || w->GetUsingFlag())continue;
        w->GetParamData(&id,NULL,NULL);if(id!=type)continue;
        float x,y,z,rx;w->GetPosData(&x,&y,&z,&rx);
        ObjMgr.GetPlayerHumanObject()->SetPosData(x,y,z,rx);
        return i;
    }
    return -1;
}
// Render the live mission from an arbitrary camera (paused; resume with
// qa_resume_game). Camera yaw uses the engine's first-person convention:
// the player-facing rotation r maps to camera_rx = pi/2 - r.
extern "C" EMSCRIPTEN_KEEPALIVE int qa_view(float x, float y, float z, float r, float pitch) {
    emscripten_pause_main_loop();
    d3dg.StartRender();
    d3dg.SetFog(true,1);
    d3dg.SetCamera(x,y,z,(float)M_PI/2-r,pitch,VIEWANGLE_NORMAL);
    d3dg.ResetZbuffer();
    d3dg.ResetWorldTransform();
    d3dg.RenderMapdata(false);
    ObjMgr.Render(x,y,z,(float)M_PI/2-r,pitch,0,false);
    d3dg.EndRender();
    return 1;
}
extern "C" EMSCRIPTEN_KEEPALIVE void qa_teleport(float x, float y, float z) {
    float ox,oy,oz,rx; human* me=ObjMgr.GetPlayerHumanObject(); me->GetPosData(&ox,&oy,&oz,&rx);
    me->SetPosData(x,y,z,rx);
}
// Aim the player's real weapon at a body height on a human and fire once
// through ObjectManager::ShotWeapon (real projectile, hit zones, damage).
extern "C" EMSCRIPTEN_KEEPALIVE int qa_aim_fire_at(int id, float height) {
    human* me=ObjMgr.GetPlayerHumanObject(); human* t=ObjMgr.GetHumanObject(id);
    if(!t || !t->GetEnableFlag())return -1;
    float x,y,z,tx,ty,tz; me->GetPosData(&x,&y,&z,nullptr); t->GetPosData(&tx,&ty,&tz,nullptr);
    float dx=tx-x, dz=tz-z, dy=(ty+height)-(y+VIEW_HEIGHT);
    me->SetRxRy(atan2f(dx,dz), atan2f(dy,sqrtf(dx*dx+dz*dz)));
    return ObjMgr.ShotWeapon(ObjMgr.GetPlayerID());
}
extern "C" EMSCRIPTEN_KEEPALIVE int qa_human_hp(int id) {
    human* h=ObjMgr.GetHumanObject(id); return (h && h->GetEnableFlag()) ? h->GetHP() : -9999;
}
extern "C" EMSCRIPTEN_KEEPALIVE int qa_player_fire() {
    return ObjMgr.ShotWeapon(ObjMgr.GetPlayerID());
}
extern "C" EMSCRIPTEN_KEEPALIVE void qa_wave_state() {
    EM_ASM({Module.qaWave=Array.of($0,$1);},ObjMgr.GetStagedCount(),ObjMgr.CountLivingHostiles());
}
extern "C" EMSCRIPTEN_KEEPALIVE void qa_resume_game() {
    emscripten_resume_main_loop();
}
// Render only the sky box, exactly as gamemain does (camera-centred, scale 2),
// looking along yaw/pitch. Used to inspect face seams and cloud layout.
extern "C" EMSCRIPTEN_KEEPALIVE int qa_sky_preview(float yaw, float pitch) {
    int model=-1,texture=-1;
    Resource.GetSkyModelTexture(&model,&texture);
    if(model==-1||texture==-1)return 0;
    emscripten_pause_main_loop();
    d3dg.StartRender();
    d3dg.SetFog(false,0);
    glClearColor(1,0,1,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    d3dg.SetCamera(0,0,0,yaw,pitch,VIEWANGLE_NORMAL);
    d3dg.SetWorldTransform(0.0f,0.0f,0.0f,0.0f,0.0f,2.0f);
    d3dg.RenderModel(model,texture,false,false,false);
    d3dg.EndRender();
    return 1;
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
// Inspect actual engine bullet rendering and the loaded colour textures.
extern "C" EMSCRIPTEN_KEEPALIVE int qa_bolt_preview(int type) {
    int model=-1,texture=-1;
    Resource.GetBulletModelTexture(type,&model,&texture);
    if(model<0 || texture<0)return 0;
    bullet bolt; bolt.SetModel(model,3.0f); bolt.SetTexture(texture);
    bolt.SetPosData(0,0,0,0,0); bolt.SetEnableFlag(true);
    bolt.SetParamData(1,0,0,0,0,0,true); bolt.ProcessObject(); // Render skips the muzzle's first tick.
    emscripten_pause_main_loop();
    d3dg.StartRender(); d3dg.SetFog(false,0);
    glClearColor(.08f,.08f,.12f,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    d3dg.SetCamera(6,3,10,-2.1f,-.2f,VIEWANGLE_NORMAL);
    bolt.Render(&d3dg,false); d3dg.EndRender();
    return 1;
}
extern "C" EMSCRIPTEN_KEEPALIVE int qa_cow_preview() {
    smallobject* cow=ObjMgr.GetSmallObject(0);
    if(!cow || !cow->GetEnableFlag())return 0;
    human* player=ObjMgr.GetPlayerHumanObject();
    float x,y,z,px,py,pz,rx;
    cow->GetPosData(&x,&y,&z,nullptr); player->GetPosData(&px,&py,&pz,&rx);
    emscripten_pause_main_loop();
    player->SetPosData(x+19,y,z,0);
    d3dg.StartRender(); d3dg.SetFog(false,0);
    glClearColor(.18f,.20f,.23f,1); glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    d3dg.SetCamera(x+35,y+15,z+48,-2.20f,0,VIEWANGLE_NORMAL);
    cow->Render(&d3dg,false); player->Render(&d3dg,&Resource,false,false,false);
    d3dg.EndRender(); player->SetPosData(px,py,pz,rx);
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
