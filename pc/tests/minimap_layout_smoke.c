#include "pc_minimap.h"
#include "pc_minimap_style.h"
#include "pc_minimap_symbols.h"
#include "m_field_make.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static void require(int ok,const char* message) {
    if (!ok) { fprintf(stderr,"FAIL: %s\n",message); exit(1); }
}
static int close_to(float a,float b) { return fabsf(a-b)<0.002f; }
int main(void) {
    const int outputs[][2]={{640,480},{1920,1080},{3440,1440},{3840,1080},{1080,1920}};
    for (int i=0;i<5;i++) for (int size=0;size<3;size++) {
        PCMinimapLayout m,nw,se;
        int w=outputs[i][0],h=outputs[i][1];
        require(pc_minimap_layout(&m,w,h,size,BLOCK_X_NUM/2.0f,(PC_MINIMAP_ROWS+PC_MAP_NORTH_CROP)/2.0f),"valid town position");
        require(m.left>=0&&m.left+m.width<320&&m.top+m.height<240,"map within screen");
        require(close_to(m.width*w/320/m.view_w,m.height*h/240/m.view_h),"acre squares retain physical aspect");
        require(close_to((320-m.left-m.width)*w/320,6.0f*h/240),"constant right-edge margin");
        require(m.marker_x>=m.left && m.marker_x<=m.left+m.width && m.marker_y>=m.top && m.marker_y<=m.top+m.height,"player remains inside local map");
        pc_minimap_layout(&nw,w,h,size,0,PC_MAP_NORTH_CROP);
        pc_minimap_layout(&se,w,h,size,BLOCK_X_NUM-0.001f,PC_MINIMAP_ROWS-0.001f);
        require(close_to(nw.marker_x,m.left)&&close_to(nw.marker_y,m.top),"northwest town boundary");
        require(se.marker_x>se.left+se.width*0.99f&&se.marker_y>se.top+se.height*0.99f&&
                se.marker_x<se.left+se.width&&se.marker_y<se.top+se.height,"southeast town boundary");
    }
    PCMinimapLayout m;
    require(!pc_minimap_layout(&m,640,480,0,BLOCK_X_NUM,1),"reject east border");
    require(!pc_minimap_layout(&m,640,480,0,1,PC_MINIMAP_ROWS),"reject south border");
    require(!pc_minimap_layout(&m,640,480,0,-1,1),"reject non-town position");
    require(!pc_minimap_layout(&m,0,480,0,1,1),"reject unavailable output");
    require(!pc_minimap_layout(&m,640,480,0,NAN,1),"reject non-finite position");
    require(pc_minimap_layout(&m,640,480,99,1,1)&&close_to(m.height,fminf(60.0f,110.0f*m.view_h/m.view_w)),"invalid size falls back to small");
    if(BLOCK_X_NUM>7) {
        PCMinimapLayout before,after;
        pc_minimap_layout(&before,1920,1080,0,13.999f,6.999f);
        pc_minimap_layout(&after,1920,1080,0,14.001f,7.001f);
        require(fabsf(after.view_x-before.view_x)<.003f && fabsf(after.view_z-before.view_z)<.003f,
                "view crosses acre boundaries continuously");
        require(close_to(after.marker_x,after.left+after.width*.5f) &&
                close_to(after.marker_y,after.top+after.height*.5f),"player centered away from world edges");
        PCMinimapLayout west,center,east;
        require(pc_minimap_layout(&west,1920,1080,0,3.5f,4),"west region visible");
        require(pc_minimap_layout(&center,1920,1080,0,10.5f,4),"center region visible");
        require(pc_minimap_layout(&east,1920,1080,0,17.5f,4),"east region visible");
        require(west.view_x<center.view_x && center.view_x<east.view_x,"local map follows all three regions");
        require(west.view_w==9 && west.view_h==7.25f,"player cannot see the full development world");
        require(pc_minimap_layout(&m,1920,1080,0,7,6.2f),"western passage visible");
        require(pc_minimap_layout(&m,1920,1080,0,14,6.2f),"eastern passage visible");
    }
    PCMinimapLayout small,large;
    pc_minimap_layout(&small,1920,1080,0,3,4);pc_minimap_layout(&large,1920,1080,2,3,4);
    require(large.width>small.width && large.height>small.height,"large world sizes remain distinct");
    require(pc_minimap_symbol_type(SHOP3)==PC_MAP_SHOP,"shop upgrades share symbol");
    require(pc_minimap_symbol_type(NPC_HOUSE000)==PC_MAP_VILLAGER,"villager house symbol");
    require(pc_minimap_symbol_type(DUMMY_NPC_HOUSE_000)==PC_MAP_VILLAGER,"generated house symbol");
    require(pc_minimap_symbol_type(POST_OFFICE)==PC_MAP_POST,"post office symbol");
    require(pc_minimap_symbol_type(MUSEUM)==PC_MAP_MUSEUM,"museum symbol");
    require(pc_minimap_symbol_type(TRAIN_STATION)==PC_MAP_STATION,"station symbol");
    require(pc_minimap_symbol_type(0)==-1,"empty ground has no symbol");
#ifdef ACO_DEV_EAST
    require(PC_MINIMAP_CAPTURE+BLOCK_X_NUM*PC_MINIMAP_ROWS<PC_MINIMAP_CAPTURE_END,"expanded atlas tile tags cannot collide with capture-end tag");
    require(pc_minimap_layout(&m,1920,1080,0,ACO_TOWN_X0+18.5f,12.5f),"offshore east island visible at true coordinates");
    require(pc_minimap_layout(&m,1920,1080,0,10.5f,14.5f),"deep southern sea visible locally");
    require(m.view_z>7 && m.view_x>0 && m.view_w<BLOCK_X_NUM && m.view_h<PC_MINIMAP_ROWS,"southern view remains local");
    require(pc_minimap_symbol_type(COTTAGE_MY)==PC_MAP_HOME,"island cottage symbol");
#endif
    require(!pc_minimap_layout(&m,640,480,0,3,0.5f),"off-map northern river source cropped");
    puts("PASS: terrain minimap layout, world borders, three region markers, aspect ratios and distinct sizes");
    return 0;
}
