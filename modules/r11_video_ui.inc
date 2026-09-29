static uint16_t xdata video_ui_page=0xffff, video_ui_pending, video_ui_target;
static uint8_t xdata video_ui_enter, video_ui_known, video_ui_query, video_title_dirty, video_resume_fallback;
static uint32_t xdata video_ui_sent;
static uint32_t VideoTime(void)
{
    uint32_t t; uint8_t enabled=ET0; ET0=0; t=GetSysTick(); ET0=enabled; return t;
}
static void VideoVp(uint16_t a,uint16_t v) { write_dgus_vp(a,(uint8_t*)&v,1); }
static void VideoSend(uint8_t cmd,uint16_t serial)
{
    uint8_t buf[3]; buf[0]=MP4; buf[1]=(uint8_t)(serial>>8); buf[2]=(uint8_t)serial;
    T5lSendUartDataToR11(cmd,buf);
    video_ui_pending=cmd; video_ui_target=serial; video_ui_sent=VideoTime();
    VideoVp(VIDEO_UI_STATUS_ADDR,1);
}
void R11VideoUiReset(void)
{
    uint8_t empty[MAX_MP3_NAME_LEN];
    video_ui_page=0xffff; video_ui_pending=video_ui_enter=video_ui_known=video_ui_query=0;
    VideoVp(VIDEO_UI_STATUS_ADDR,2); VideoVp(VIDEO_UI_COUNT_ADDR,0);
    VideoVp(VIDEO_UI_BUTTON_ADDR,2);
    r11_player.state=0; r11_player.total_serial=0;
    VideoVp(PLAY_STATUS_ADDR,0);
    memset(empty,0,sizeof(empty));
    write_dgus_vp(MP4_NOW_PLAY_NAME_ADDR,empty,MAX_MP3_NAME_LEN>>1);
    video_title_dirty=1;
    video_resume_fallback=0;
    page_st.video_flag=page_st.fullvideo_flag=0x5a;
    page_st.video_page=120; page_st.fullvideo_page=121;
    mainview.video_x_point=16; mainview.video_y_point=64;
    mainview.video_high=992; mainview.video_weight=448;
}
void R11VideoUiTask(void)
{
    uint16_t p=ReadPageId(), count=r11_player.total_serial, desired;
    uint8_t active=(p==120 || p==121), was=(video_ui_page==120 || video_ui_page==121);
    uint8_t query[2];
    if(count>256) count=256;
    VideoVp(VIDEO_UI_COUNT_ADDR,count);
    VideoVp(VIDEO_UI_BUTTON_ADDR,video_ui_pending?2:(r11_player.state==1?0:(r11_player.state==4?1:2)));
    if(p!=video_ui_page) {
        if(active && !was) { video_ui_enter=1; video_ui_query=0; video_resume_fallback=0; }
        if(!active && was) { video_ui_enter=0; VideoSend(cmdMP4_PAUSE,0); }
        if(p==121) { R11ChangePictureLocate(0,0,1024,600,1); Big_Small_Flag=1; }
        else if(p==120) { R11ChangePictureLocate(16,64,992,448,0); Big_Small_Flag=0; }
        else if(p==9) R11ChangePictureLocate(16,116,562,434,0);
        else R11ChangePictureLocate(1024,600,1,1,0);
        VideoVp(BIG_SMALL_FLAG_ADDR,p==121); R11ClearPicture(0); video_ui_page=p;
    }
    if(video_ui_pending && VideoTime()-video_ui_sent>=1500UL) {
        if(video_ui_pending==cmdMP4_REPLAY && !video_resume_fallback && active) {
            video_ui_known=0; video_ui_query=0; video_ui_enter=1; video_resume_fallback=1;
            video_ui_pending=0;
        } else {
        video_ui_pending=0; video_ui_enter=0; video_title_dirty=0; VideoVp(VIDEO_UI_STATUS_ADDR,2);
        if(p==121) SwitchPageById(120);
        }
    }
    if(video_ui_enter && !video_ui_pending) {
        if(!video_ui_query) {
            query[0]=r11_player.store_type; query[1]=MP4;
            if(query[0]<1 || query[0]>3) query[0]=r11_player.store_type=exUDISK;
            T5lSendUartDataToR11(cmdMP4_UPDATEFILE,query);
            video_ui_query=1; video_ui_pending=cmdMP4_UPDATEFILE; video_ui_sent=VideoTime();
            VideoVp(VIDEO_UI_STATUS_ADDR,1);
        } else if(count) {
            video_ui_enter=0;
            if(video_ui_known && r11_player.now_play_serial<count && r11_player.state==4) VideoSend(cmdMP4_REPLAY,0);
            else if(video_ui_known && r11_player.now_play_serial<count && r11_player.state==1) VideoVp(VIDEO_UI_STATUS_ADDR,0);
            else VideoSend(cmdMP4_PLAY_BY_NUM,0);
        } else { video_ui_enter=0; VideoVp(VIDEO_UI_STATUS_ADDR,3); }
    }
    /* Resolve the title using the existing five-file list protocol. Paging is
       asynchronous, and never changes the actual playback serial or position. */
    if(active && video_ui_known && !video_ui_enter && !video_ui_pending && video_title_dirty && count) {
        desired=r11_player.now_play_serial/5+1;
        if(r11_player.now_play_page!=desired) {
            VideoSend(r11_player.now_play_page<desired?cmdMP4_NEXTFILE:cmdMP4_PREVFILE,0);
        } else {
            desired=r11_player.now_play_serial%5;
            if(desired<r11_player.page_mp4_nums)
                write_dgus_vp(MP4_NOW_PLAY_NAME_ADDR,mp4_name[desired],MAX_MP3_NAME_LEN>>1);
            video_title_dirty=0;
        }
    }
}
static void VideoFilesReported(uint8_t cmd)
{
    if(video_ui_pending==cmd) {
        if(cmd==cmdMP4_NEXTFILE) ++r11_player.now_play_page;
        if(cmd==cmdMP4_PREVFILE && r11_player.now_play_page>1) --r11_player.now_play_page;
        video_ui_pending=0;
        VideoVp(VIDEO_UI_STATUS_ADDR,r11_player.total_serial?0:3);
    }
    video_title_dirty=1;
    if(!r11_player.total_serial && ReadPageId()==121) SwitchPageById(120);
}
static void VideoStateReported(uint8_t state)
{
    r11_player.state=state;
    VideoVp(PLAY_STATUS_ADDR,state);
    if((video_ui_pending==cmdMP4_PAUSE && state==4) ||
       (video_ui_pending==cmdMP4_REPLAY && state==1) ||
       (video_ui_pending==cmdMP4_PLAY_BY_NUM && state==1 && video_ui_known && r11_player.now_play_serial==video_ui_target)) {
        video_ui_pending=0; VideoVp(VIDEO_UI_STATUS_ADDR,0);
    }
}
static void VideoSerialReported(uint16_t serial)
{
    /* B1 is an 8-bit index in the installed R11 protocol (0..255). */
    if(serial>255) { VideoVp(VIDEO_UI_STATUS_ADDR,2); return; }
    r11_player.now_play_serial=serial; video_ui_known=1; video_title_dirty=1;
    VideoVp(NP4_NOW_NUM_ADDR,serial);
    if(video_ui_pending==cmdMP4_PLAY_BY_NUM && serial==video_ui_target && r11_player.state==1) {
        video_ui_pending=0; VideoVp(VIDEO_UI_STATUS_ADDR,0);
    }
}
static void VideoMediaChanged(void)
{
    uint8_t empty[MAX_MP3_NAME_LEN];
    memset(empty,0,sizeof(empty));
    write_dgus_vp(MP4_NOW_PLAY_NAME_ADDR,empty,MAX_MP3_NAME_LEN>>1);
    video_ui_known=0; r11_player.total_serial=0; r11_player.state=0;
    video_ui_pending=0; video_ui_query=0; video_title_dirty=1;
    video_ui_enter=(ReadPageId()==120 || ReadPageId()==121);
    VideoVp(PLAY_STATUS_ADDR,0); VideoVp(VIDEO_UI_STATUS_ADDR,2);
}
static uint8_t VideoUiKey(uint16_t key)
{
    uint16_t count=r11_player.total_serial, next;
    if(key!=keyMP4_PREVIOUS_VIDEO && key!=keyMP4_NEXT_VIDEO && key!=keyMP4_TOGGLE_PAUSE &&
       key!=keyMP4_IMG_SET_BIG && key!=keyMP4_IMG_SET_SMALL && key!=keyMP4_PAUSE && key!=keyMP4_REPLAY) return 0;
    if(key==keyMP4_IMG_SET_SMALL) { SwitchPageById(120); return 1; }
    if(key==keyMP4_IMG_SET_BIG) { if(count) SwitchPageById(121); return 1; }
    if(ReadPageId()!=120 && ReadPageId()!=121) return 1;
    if(video_ui_pending || video_ui_enter) return 1;
    if(count>256) count=256;
    if(!count) { VideoVp(VIDEO_UI_STATUS_ADDR,3); return 1; }
    if(key==keyMP4_PREVIOUS_VIDEO || key==keyMP4_NEXT_VIDEO) {
        if(count<2 || !video_ui_known || r11_player.now_play_serial>=count) return 1;
        next=key==keyMP4_NEXT_VIDEO?(r11_player.now_play_serial+1)%count:
            (r11_player.now_play_serial? r11_player.now_play_serial-1:count-1);
        VideoSend(cmdMP4_PLAY_BY_NUM,next);
    } else if(key==keyMP4_TOGGLE_PAUSE) {
        if(r11_player.state==1) VideoSend(cmdMP4_PAUSE,0);
        else if(r11_player.state==4) VideoSend(cmdMP4_REPLAY,0);
        else { video_ui_enter=1; video_ui_query=0; }
    } else VideoSend(key==keyMP4_PAUSE?cmdMP4_PAUSE:cmdMP4_REPLAY,0);
    return 1;
}
