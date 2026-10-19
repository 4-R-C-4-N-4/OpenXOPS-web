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
    std::setvbuf(stdout,nullptr,_IONBF,0);
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
    for(int type=44;type<=46;type++) {
        for(int trial=0;trial<32;trial++) {
            human alien(&params);
            alien.SetParamData(type,0,1,1,true);
            alien.HitBulletUp(64);
            assert(alien.GetHP()>0); // Clean body hits still need follow-up shots.
            alien.SetParamData(type,0,1,1,true);
            alien.HitBulletHead(64);
            assert(alien.GetHP()<=0); // Every alien dies to one clean lever headshot.
        }
    }
    std::puts("ONE-SHOT LEVER HEADSHOTS OK");

    // UFO ESCAPE: a single thrown MOON ROCK kills the first guard only with a headshot.
    assert(params.GetWeapon(27,&w)==0 && strcmp(w.name,"MOON ROCK")==0);
    assert(w.throwaway && w.nbsmax==1 && w.bullettype==5 && w.attacks==45);
    assert(params.GetHuman(47,&h)==0 && h.Weapon[0]==27 && h.Weapon[1]==ID_WEAPON_NONE && h.texture==30);
    BulletParameter rockbolt;
    assert(params.GetBullet(5,&rockbolt)==0 && strcmp(rockbolt.model,"./data/model/weapon/rock.x")==0);
    for(int trial=0;trial<32;trial++) {
        human guard(&params);
        guard.SetParamData(44,0,1,1,true); guard.HitBulletHead(w.attacks); assert(guard.GetHP()<=0);
        guard.SetParamData(44,0,1,1,true); guard.HitBulletUp(w.attacks); assert(guard.GetHP()>0);
        guard.SetParamData(44,0,1,1,true); guard.HitBulletLeg(w.attacks); assert(guard.GetHP()>0);
    }
    for(int i=0;i<=26;i++){ assert(params.GetWeapon(i,&w)==0 && w.throwaway==false); }
    std::puts("MOON ROCK PARAMETERS OK");
    std::puts("PARAMETERS OK");

    // Free missions belong in the main list, including MIF-backed missions.
    char missionname[64], briefingpath[MAX_PATH];
    assert(params.GetOfficialMissionCount()==2);
    assert(params.GetOfficialMission(0,missionname,NULL,NULL,NULL,NULL,NULL)==0);
    assert(strcmp(missionname,"UFO FIELD")==0);
    assert(params.GetOfficialMissionBriefingPath(0,briefingpath)==0);
    assert(strcmp(briefingpath,"data\\ufofield\\ufo.mif")==0);
    assert(params.GetOfficialMissionBriefingPath(1,briefingpath)==0);
    assert(strcmp(briefingpath,"data\\starter\\start.txt")==0);
    std::puts("UFO FIELD IN MAIN MISSION LIST OK");

    D3DGraphics graphics;
    ResourceManager resources; resources.SetParameterInfo(&params); resources.SetD3DGraphics(&graphics);
    SoundControl device;
    SoundManager sound(&device,&resources,&params);
    BlockDataInterface blocks; PointDataInterface points; Collision collision; MIFInterface mif;
    ObjectManager objects;
    objects.SetClass(&params,&graphics,&resources,&blocks,&points,&collision,&sound,&mif);
    assert(mif.LoadFiledata("data/ufofield/ufo.mif", true)==0);
    assert(points.LoadFiledata("data/ufofield/ufo.pd1")==0);
    objects.LoadPointData();
    assert(objects.GetStagedCount()==20 && objects.CountLivingHostiles()==0);
    assert(objects.CheckGameOverorComplete()==0);
    EventControl events(&points,&objects); events.Reset(-100);
    EventControl second(&points,&objects); second.Reset(-110);
    EventControl third(&points,&objects); third.Reset(-120);
    EventParameter state={};
    auto processEvents=[&]() {
        events.ProcessEventPoint(false,&state);
        second.ProcessEventPoint(false,&state);
        third.ProcessEventPoint(false,&state);
    };
    for(int wave=1;wave<=3;wave++) {
        const int expected=wave==2?8:6;
        for(int tick=0;tick<200 && objects.CountLivingHostiles()!=expected;tick++)
            processEvents();
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
        for(int tick=0;tick<10;tick++) processEvents();
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

    // Arrival cues must never make aliens investigate their own spawn.
    sound.InitWorldSound();
    sound.Teleport(0,32,-112,1);
    sound.PlayWorldSound(0,32,-112,0,1);
    soundlist heard[MAX_SOUNDMGR_LIST];
    assert(sound.GetWorldSound(0,32,-112,1,heard)==0);
    std::puts("ARRIVAL SOUND DOES NOT ALERT AI OK");

    // Exercise real AI navigation and collision, not just parsed waypoint ids.
    assert(blocks.LoadFiledata("data/ufofield/temp.bd1")==0);
    blocks.CalculationBlockdata(false);
    collision.InitCollision(&blocks);
    assert(points.LoadFiledata("data/ufofield/ufo.pd1")==0);
    objects.LoadPointData();
    assert(objects.DeployStagedGroup(1)==6);
    AIcontrol bots[6];
    for(int i=0;i<6;i++) {
        bots[i].SetClass(&objects,i+1,&blocks,&points,&params,&collision,&sound);
        bots[i].Init(); bots[i].SetNoFightFlag(true);
    }
    for(int tick=0;tick<400;tick++) {
        objects.Process(-1,false,false);
        for(auto& bot:bots) bot.Process();
        sound.PlayWorldSound(0,20,200,0,0);
    }
    int descended=0;
    for(int i=1;i<=6;i++) {
        float x,y,z;objects.GetHumanObject(i)->GetPosData(&x,&y,&z,nullptr);
        std::printf("ALIEN %d POS %.1f %.1f %.1f\n",i,x,y,z);
        if(y<5 && z>-40) descended++;
    }
    assert(descended>=4);
    std::puts("RAMP DESCENT OK");
    objects.Cleanup();

    // Combat-pressure probe: an exposed rancher standing still in the open
    // while one live wave hunts with real AI, weapons and collision.
    auto pressure=[&](int wave, unsigned seed) {
        srand(seed);
        objects.LoadPointData();
        int ids[8], n=0;
        for(int i=0;i<MAX_HUMAN;i++) objects.TakeAIInitFlag(i);
        n=objects.DeployStagedGroup(wave);
        for(int i=0,k=0;i<MAX_HUMAN && k<n;i++) if(objects.TakeAIInitFlag(i)) ids[k++]=i;
        human* player=objects.GetPlayerHumanObject();
        player->SetPosData(100,0,170,(float)M_PI/2);
        AIcontrol ai[8];
        for(int k=0;k<n;k++){ ai[k].SetClass(&objects,ids[k],&blocks,&points,&params,&collision,&sound); ai[k].Init(); }
        int tick=0;
        for(;tick<1000 && player->GetHP()>0;tick++){
            objects.Process(-1,false,false);
            for(int k=0;k<n;k++) ai[k].Process();
            sound.PlayWorldSound(100,20,170,0,0);
        }
        objects.Cleanup();
        return tick;
    };
    for(int wave=1;wave<=3;wave++){
        int total=0, fastest=100000;
        for(unsigned seed=1;seed<=5;seed++){ int t=pressure(wave,seed); total+=t; if(t<fastest) fastest=t; }
        std::printf("PRESSURE WAVE %d: exposed death after mean %.1fs, fastest %.1fs\n",wave,total/5*0.03,fastest*0.03);
        assert(total/5*0.03>=10.0 && fastest*0.03>=7.0); // Playtest: aliens previously killed in ~1-2s of contact.
    }
    std::puts("EXPOSED PLAYER SURVIVAL OK");

    // A chest-height laser must stop at a cow body, not pass through a tiny
    // ground-level legacy sphere. Removing the cow must reopen the shot.
    objects.LoadPointData();
    for(int i=0;i<MAX_SMALLOBJECT;i++) objects.GetSmallObject(i)->SetEnableFlag(false);
    smallobject* cover=objects.GetSmallObject(0);
    cover->SetPosData(0,0,0,0); cover->SetEnableFlag(true);
    int empty[2]={0,0};
    int targetid=objects.AddHumanIndex(15,0,0,0,43,1,empty,30,false);
    assert(targetid>0);
    auto shoot=[&](int index, float z=0, float height=16) {
        human* target=objects.GetHumanObject(targetid); target->SetHP(120);
        target->SetPosData(15,height-16,z,0);
        bullet* shot=objects.GetBulletObject(index);
        shot->SetPosData(-15,height,z,0,0); shot->SetParamData(50,0,40,0,0,0,true); shot->SetEnableFlag(true);
        objects.Process(-1,false,false);
        return target->GetHP();
    };
    assert(shoot(0)==120);
    assert(shoot(1,8)==120); // Along the cow's long body.
    cover->SetPosData(0,0,0,(float)M_PI/2);
    assert(shoot(2,8)<120);  // Outside the now-rotated body's narrow side.
    assert(shoot(3)==120);
    assert(shoot(4,0,26)<120); // A shot above the back is not blocked.
    cover->HitBullet(1000);
    assert(shoot(5)<120); // Destroyed cows leave no invisible cover.
    assert(mif.LoadFiledata("data/starter/start.txt",false)==0);
    assert(mif.GetAddSmallobjectCover(0)==nullptr); // No hull leaks into classic data.
    std::puts("COW BODY STOPS LASERS OK");
    objects.Cleanup();

    // A thrown rock empties the hand: no reserve, slot freed for a laser pickup.
    objects.LoadPointData();
    int rockonly[2]={27,ID_WEAPON_NONE};
    int captive=objects.AddHumanIndex(0,0,180,0,47,0,rockonly,7,false);
    assert(captive>=0);
    weapon* held[TOTAL_HAVEWEAPON]; int loaded[TOTAL_HAVEWEAPON], total[TOTAL_HAVEWEAPON], selected=0;
    objects.GetHumanObject(captive)->GetWeapon(&selected,held,loaded,total);
    int rockid=-1,rockloaded=-1,rocktotal=-1;
    assert(held[0]!=nullptr); held[0]->GetParamData(&rockid,&rockloaded,&rocktotal);
    assert(rockid==27 && rockloaded==1 && rocktotal==1);
    for(int i=0;i<MAX_BULLET;i++) objects.GetBulletObject(i)->SetEnableFlag(false);
    objects.ShotWeapon(captive);
    objects.GetHumanObject(captive)->GetWeapon(&selected,held,loaded,total);
    assert(held[0]==nullptr && held[1]==nullptr);
    int flying=-1;
    for(int i=0;i<MAX_BULLET;i++) if(objects.GetBulletObject(i)->GetEnableFlag()) flying=i;
    assert(flying>=0 && objects.IsBulletHiddenInFirstPerson(flying)==false); // The rock stays visible.
    std::puts("THROWN ROCK EMPTIES HAND OK");

    // The player's own lasers are seen end-on in first person and cover the
    // crosshair, so they are hidden there; other shooters' lasers are not.
    int zap[2]={24,ID_WEAPON_NONE};
    int alien=objects.AddHumanIndex(30,0,180,0,44,1,zap,8,false);
    human* me=objects.GetPlayerHumanObject();
    me->SetWeapon(held,loaded,total); // drop whatever the map gave; give a zap
    assert(objects.CheatNewWeapon(objects.GetPlayerID(),24));
    { int s; weapon* hw[TOTAL_HAVEWEAPON]; me->GetWeapon(&s,hw,NULL,NULL); assert(hw[s]); hw[s]->SetParamData(24,12,36,false); }
    auto fired=[&](int shooter){
        for(int i=0;i<MAX_BULLET;i++) objects.GetBulletObject(i)->SetEnableFlag(false);
        for(int t=0;t<40;t++){ if(objects.ShotWeapon(shooter)) break; objects.Process(-1,false,false); }
        for(int i=0;i<MAX_BULLET;i++) if(objects.GetBulletObject(i)->GetEnableFlag()) return i;
        return -1;
    };
    int mine=fired(objects.GetPlayerID());
    assert(mine>=0 && objects.IsBulletHiddenInFirstPerson(mine));
    int theirs=fired(alien); assert(theirs>=0 && objects.IsBulletHiddenInFirstPerson(theirs)==false);
    std::puts("OWN LASER HIDDEN IN FIRST PERSON OK");
    objects.Cleanup();
    return 0;
}
