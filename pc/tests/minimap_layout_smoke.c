#include "pc_minimap.h"
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
        require(pc_minimap_layout(&m,w,h,size,BLOCK_X_NUM/2.0f,PC_MINIMAP_ROWS/2.0f),"valid town position");
        require(m.left>=0&&m.left+m.width<320&&m.top+m.height<240,"map within screen");
        require(close_to(m.width*w/320/BLOCK_X_NUM,m.height*h/240/PC_MINIMAP_ROWS),"acre squares retain physical aspect");
        require(close_to((320-m.left-m.width)*w/320,6.0f*h/240),"constant right-edge margin");
        require(close_to(m.marker_x,m.left+m.width/2)&&close_to(m.marker_y,m.top+m.height/2),"town center maps to center");
        pc_minimap_layout(&nw,w,h,size,0,0);
        pc_minimap_layout(&se,w,h,size,BLOCK_X_NUM-0.001f,PC_MINIMAP_ROWS-0.001f);
        require(close_to(nw.marker_x,m.left)&&close_to(nw.marker_y,m.top),"northwest town boundary");
        require(se.marker_x>m.left+m.width*0.99f&&se.marker_y>m.top+m.height*0.99f&&
                se.marker_x<m.left+m.width&&se.marker_y<m.top+m.height,"southeast town boundary");
    }
    PCMinimapLayout m;
    require(!pc_minimap_layout(&m,640,480,0,BLOCK_X_NUM,1),"reject east border");
    require(!pc_minimap_layout(&m,640,480,0,1,PC_MINIMAP_ROWS),"reject south border");
    require(!pc_minimap_layout(&m,640,480,0,-1,1),"reject non-town position");
    require(!pc_minimap_layout(&m,0,480,0,1,1),"reject unavailable output");
    require(!pc_minimap_layout(&m,640,480,0,NAN,1),"reject non-finite position");
    require(pc_minimap_layout(&m,640,480,99,1,1)&&close_to(m.height,fminf(60.0f,110.0f*PC_MINIMAP_ROWS/BLOCK_X_NUM)),"invalid size falls back to small");
    if(BLOCK_X_NUM>7) {
        PCMinimapLayout west,center,east;
        require(pc_minimap_layout(&west,1920,1080,0,3.5f,4),"west region visible");
        require(pc_minimap_layout(&center,1920,1080,0,10.5f,4),"center region visible");
        require(pc_minimap_layout(&east,1920,1080,0,17.5f,4),"east region visible");
        require(west.marker_x<center.marker_x && center.marker_x<east.marker_x,"all three regions in geographic order");
        require(pc_minimap_layout(&m,1920,1080,0,7,6.2f),"western passage visible");
        require(pc_minimap_layout(&m,1920,1080,0,14,6.2f),"eastern passage visible");
    }
    PCMinimapLayout small,large;
    pc_minimap_layout(&small,1920,1080,0,3,4);pc_minimap_layout(&large,1920,1080,2,3,4);
    require(large.width>small.width && large.height>small.height,"large world sizes remain distinct");
    puts("PASS: terrain minimap layout, world borders, three region markers, aspect ratios and distinct sizes");
    return 0;
}
