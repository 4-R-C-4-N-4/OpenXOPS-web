// Actual engine integration proof; only rendering, input, window and audio
// devices are stubbed. Keep upstream globals/utilities, but do not run GameEntry.
#define main openxops_original_entry
#include "../../src/main.cpp"
#undef main
#include "../../src/gamemain.h"
#include <cassert>
#include <cstdio>

static int kill_hostiles(ObjectManager& objects) {
    int team=0, count=0;
    objects.GetPlayerHumanObject()->GetParamData(nullptr,nullptr,nullptr,&team);
    for(int i=0;i<MAX_HUMAN;i++) {
        human* h=objects.GetHumanObject(i);
        int other=0; h->GetParamData(nullptr,nullptr,nullptr,&other);
        if(h->GetEnableFlag() && h->GetHP()>0 && team!=other) {
            h->SetHP(0); count++;
        }
    }
    return count;
}

int main(int argc, char** argv) {
    assert(argc==2);
    InitRand();
    vfs_init_from_args(argv[1]);
    ParameterInfo params; params.InitInfo();
    WeaponParameter w;
    assert(params.GetWeapon(23,&w)==0 && w.attacks==64 && w.nbsmax==8 && w.scopemode==1);
    assert(params.GetWeapon(10,&w)==0 && w.attacks==65 && w.bullettype==0); // AK47
    assert(params.GetWeapon(11,&w)==0 && w.bullettype==0);
    for(int i=24;i<=26;i++) {
        assert(params.GetWeapon(i,&w)==0 && w.bullettype==i-22 && w.yakkyou_size==0);
    }
    assert(params.GetWeapon(53,&w)==0 && w.bullettype==0); // legacy BugWeapon fallback
    HumanParameter h;
    for(int i=43;i<=46;i++) {
        assert(params.GetHuman(i,&h)==0 && h.Weapon[0]==i-20 && h.Weapon[1]==ID_WEAPON_NONE);
    }
    std::puts("PARAMETERS OK");

    D3DGraphics graphics;
    ResourceManager resources; resources.SetParameterInfo(&params); resources.SetD3DGraphics(&graphics);
    SoundManager sound;
    BlockDataInterface blocks; PointDataInterface points; Collision collision; MIFInterface mif;
    ObjectManager objects;
    objects.SetClass(&params,&graphics,&resources,&blocks,&points,&collision,&sound,&mif);
    assert(mif.LoadFiledata("addon/ufo.mif", true)==0);
    assert(points.LoadFiledata("data/ufofield/ufo.pd1")==0);
    objects.LoadPointData();
    assert(objects.GetStagedCount()==20 && objects.CountLivingHostiles()==0);
    assert(objects.CheckGameOverorComplete()==0);
    EventControl events(&points,&objects); events.Reset(-100);
    EventParameter state={};
    for(int wave=1;wave<=3;wave++) {
        const int expected=wave==2?8:6;
        for(int tick=0;tick<200 && objects.CountLivingHostiles()!=expected;tick++)
            events.ProcessEventPoint(false,&state);
        assert(objects.CountLivingHostiles()==expected);
        assert(objects.GetStagedCount()==(wave==1?14:wave==2?6:0));
        assert(objects.CheckGameOverorComplete()==0);
        assert(objects.DeployStagedGroup(wave)==0); // deploy is idempotent
        int flags=0;
        for(int i=0;i<MAX_HUMAN;i++) {
            if(objects.TakeAIInitFlag(i)) flags++;
            assert(!objects.TakeAIInitFlag(i));
        }
        assert(flags==expected);
        const short waiting=events.GetNextP4();
        for(int tick=0;tick<10;tick++) events.ProcessEventPoint(false,&state);
        assert(events.GetNextP4()==waiting); // event 40 blocks while hostiles live
        assert(kill_hostiles(objects)==expected);
        std::printf("WAVE %d OK: killed %d, pending %d\n",wave,expected,objects.GetStagedCount());
        if(wave<3) assert(objects.CheckGameOverorComplete()==0);
    }
    assert(objects.CheckGameOverorComplete()==1);
    std::puts("FINAL COMPLETION OK");
    objects.Cleanup();
    assert(objects.GetStagedCount()==0);
    for(int i=0;i<MAX_HUMAN;i++) assert(!objects.TakeAIInitFlag(i));
    assert(points.LoadFiledata("data/starter/start.pd1")==0);
    objects.LoadPointData();
    assert(objects.GetStagedCount()==0 && objects.CheckGameOverorComplete()==0);
    assert(kill_hostiles(objects)>0);
    assert(objects.CheckGameOverorComplete()==1);
    std::puts("CLASSIC NO-STAGING COMPLETION OK");
    objects.Cleanup();
    return 0;
}
