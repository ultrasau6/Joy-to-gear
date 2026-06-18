#include <math.h>
#include <stdio.h>
#include <fcntl.h>

#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <libevdev-1.0/libevdev/libevdev.h>
#include <libevdev-1.0/libevdev/libevdev-uinput.h>

#include <gtk/gtk.h>

int activated = 0 ;

char* SelectedDev ;

guint timeout_id = 0 ;

int mainInterval = 1;

struct libevdev *dev = NULL;

int min(int a , int b){
    if (a > b ){
        return b ;
    }
    return a;
}

int max(int a , int b){
    if (a > b ){
        return a ;
    }
    return b;
}

struct input_event RumbleEffect(){

    int fd = libevdev_get_fd(dev);
    struct ff_effect effect;
    memset(&effect, 0, sizeof(effect));

    effect.type = FF_RUMBLE;
    effect.id   = -1;

    effect.u.rumble.strong_magnitude = 0xffff;
    effect.u.rumble.weak_magnitude   = 0x0000;

    effect.replay.length = 150/2;
    effect.replay.delay  = 0;
    struct input_event play;
    
    if (ioctl(fd, EVIOCSFF, &effect) < 0) {
        perror("upload FF RumbleEffect");
        return play;
    }

    
    memset(&play, 0, sizeof(play));
    play.type  = EV_FF;
    play.code  = effect.id;
    play.value = 1;//play the effect

    
    return play;
}

struct ff_effect move;
struct ff_effect move2;
struct libevdev_uinput *uidev = NULL;
struct input_event ev;
struct input_event rumble ;

int ff_initialized = 0;
int Xaxe = 0 ;
int Yaxe = 1 ;

int Xpos = 0 ;
int Ypos = 0 ;

int LXpos = 0 ;
int LYpos = 0 ;

int RXpos = 0 ;
int RYpos = 1 ;

int Size = 1024;
int borderSize = 50;
int SizeSpeed ;

int Speed = -1;
int LSpeed = -1;

int DeltaX = 0;
int DeltaY = 0;

void ChangeSettingMove(int fd){
    if (!ff_initialized) return;

    if (ioctl(fd, EVIOCSFF, &move) < 0) perror("init FF constant move"); return;
}

int ConstMove(){

    int fd = libevdev_get_fd(dev);
    
    printf("%s\n",libevdev_get_name(dev));
    
    if (!libevdev_has_event_code(dev, EV_FF, FF_CONSTANT)) {
        perror("FF_CONSTANT non supporté\n");
        return -1;
    }

    // Vérifier le support FF_RUMBLE
    if (!libevdev_has_event_code(dev, EV_FF, FF_RUMBLE)) {
        perror("FF_RUMBLE non supporté\n");
        return -1;
    }

    if (fd < 0){
        perror("fd<0\n");
        return -1;
    }
    else{
        printf("fd ok\n");
    }

    memset(&move, 0, sizeof(move));
    
    move.type = FF_CONSTANT;
    move.id = -1;
    move.replay.length        = 0xFFFF;
    move.replay.delay         = 0;
    move.u.constant.level     = 0x7FFF;
    move.direction            = 0xC000;

    move.u.constant.envelope.attack_length  =  0 ;
    move.u.constant.envelope.attack_level   =  0 ;
    move.u.constant.envelope.fade_length    = 50 ;
    move.u.constant.envelope.fade_level     =  0 ;

    struct input_event play;

    if (ioctl(fd, EVIOCSFF, &move) < 0) {
        perror("init FF constant const");
        return -1;
    }

    memset(&play, 0, sizeof(play));
    play.type  = EV_FF;
    play.code  = move.id;
    play.value = 1;
    write(fd, &play, sizeof(play));

    ff_initialized = 1;
    return 0;
}


void XCenterForce(int fd,int Xpos,int DeltaX){
    if (Xpos>0){
        move.direction = 0x4000;
        ChangeSettingMove(fd);
    }
    else {
        //printf("Xpos L : %d\n",Xpos);
        move.direction = 0xC000;
        ChangeSettingMove(fd);
    }
    
    //int force = 10000 ;
    
    int force = abs(Xpos)*(63.998046875*0.8) - (abs(DeltaX)*1000);
    force = min(max(force,0), 32767);

    if (force<0){force = 0;}
    move.u.constant.level = force;
    ChangeSettingMove(fd);
}

int init_dev(){
    printf("init\n");
    ConstMove();

    rumble = RumbleEffect();

    int fd = libevdev_get_fd(dev);
    if (fd < 0) return -1;

    if (rumble.code >= 0) write(fd, &rumble, sizeof(rumble));
    
    write(fd, &move, sizeof(move));

    SizeSpeed = ((1024/3)-borderSize);

    printf("init end\n");
    return 0;
}

gboolean gearboxFF(gpointer data){
    int fd = libevdev_get_fd(dev);
    if (fd < 0 ){
        printf("fd<0\n");
        return G_SOURCE_CONTINUE;
    }

    while (!ff_initialized){};

    int rc = libevdev_next_event(dev,
                                LIBEVDEV_READ_FLAG_NORMAL | LIBEVDEV_READ_FLAG_BLOCKING,
                                &ev);
    
    
    if (rc == LIBEVDEV_READ_STATUS_SUCCESS) {
        if (ev.type == EV_ABS) {

            DeltaX = Xpos - LXpos;
            DeltaY = Ypos - LYpos;

            if (ev.code == Xaxe){
                Xpos = ev.value;
                
            }
            else if (ev.code == Yaxe){
                Ypos = ev.value;
            }

            



            if (abs(Ypos) > 350 ){
                    move.u.constant.level = 32767/2;
                if ((Ypos)>0){
                    move.direction = 0x0000;
                    ChangeSettingMove(fd);
                }
                else {
                    move.direction = 0x8000;
                    ChangeSettingMove(fd);
                }



                RXpos = Xpos+512;RYpos = Ypos+512;
                if (Speed == -1){
                    if (Ypos>0){
                        Speed = floor(RXpos/(1024/3));
                    }
                    else{
                        Speed = floor(RXpos/(1024/3)+3);
                    }
                }
            }
            else 
            {
                Speed = -1;
                //int force = 10000 ;
                XCenterForce(fd,Xpos,DeltaX);
            }

            if (Speed!=LSpeed){
                    write(fd, &rumble, sizeof(rumble));
                    printf("speed : %d %d\n",Speed,LSpeed);
            }

            LXpos = Xpos;
            LYpos = Ypos;
            LSpeed = Speed;
        }
    }
    
    return G_SOURCE_CONTINUE;
}

int getfdFromPath(const char* path){
    int fd = open(path, O_RDWR | O_NONBLOCK);
    if (fd < 0) {
        perror("open");
        return -1;
    }
    return fd;
}

int getrcFromfd(const int fd , struct libevdev **dev){
    int rc = libevdev_new_from_fd(fd, dev);
    if (rc < 0) {
        fprintf(stderr, "Erreur libevdev: %s\n", strerror(-rc));
        return -1;
    }
    return rc;
}

char** GetDevicesNames(char* names[],int *n){
    struct libevdev *devFind = NULL;
    char* path = malloc(250);
    int peripheralFF_Number = 0;
    int peripheral_Number = 0;
    
    while (1){
        sprintf(path, "/dev/input/event%d", peripheral_Number);
        if (access(path, F_OK) == 0) {
            int fd = getfdFromPath(path);
            if (fd == -1){free(path);printf("fd -1\n");return 0;}

            int rc = getrcFromfd(fd,&devFind);
            if (rc == -1){free(path);printf("rc -1\n");return 0;}

            
            printf("FF ok\n");
            if (libevdev_has_event_code(devFind, EV_FF, FF_CONSTANT)&&
                libevdev_has_event_code(devFind, EV_FF, FF_RUMBLE))
            {
                printf("FF ok %s \n",libevdev_get_name(devFind));
                names[peripheralFF_Number] = libevdev_get_name(devFind);
                peripheralFF_Number += 1;
            }
            else{
                printf("FF no ok\n");
            }
            peripheral_Number++;

        }
        else{break;}
    }
    *n = peripheralFF_Number;
    return names;
}

int FindDeviceByName(char* name, struct libevdev **dev){

    char* path = malloc(250);

    int perNum = 0;
    while (1)
    {
        
        //create path for the peripheral
        sprintf(path, "/dev/input/event%d", perNum);
        printf("%s : ",path);
        if (access(path, F_OK) == 0) {
            // if file exists
            int fdF = getfdFromPath(path);
            if (fdF == -1){free(path);printf("fdF -1\n");return -1;}

            int rcF = getrcFromfd(fdF,dev);
            if (rcF == -1){free(path);printf("rcF -1\n");return -1;}

            printf("%s\n",libevdev_get_name(*dev));

            if (strcmp(libevdev_get_name(*dev),name) == 0){
                printf("joystick found\n");
                free(path);
                return rcF;
            }
            // if the peripheral is not the one we search, we remove it and set the counter to ++ to setup the next one
            libevdev_free(*dev);
            //*dev = NULL;
            close(fdF);
            perNum++;

        } else {
            // if file doesn't exist
            printf("file dont exist\n");
            break;
        }
        


    }
    free(path);
    return -1;
}

int rcMain;

void ChangePeripheral(GtkWidget *combo){
    printf("\n%s\n",gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(combo)));
    SelectedDev = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(combo));
    rcMain =   FindDeviceByName(SelectedDev,&dev);
    printf("%d\n",rcMain);
}

void RefreshPeripheral(GtkWidget *button,gpointer data){
    GtkWidget *combo = GTK_WIDGET(data);
    printf("Refresh\n");
    int devNumber;
    char* names[255];
    GetDevicesNames(names,&devNumber);
    gtk_combo_box_text_remove_all(GTK_COMBO_BOX_TEXT(combo));
    

    for(int i = 0 ; i < devNumber;i++){
        printf("%d:%s\n",i,names[i]);
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo), names[i]);
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(combo),0);
}

gboolean placholder(gpointer data){
    printf("placeholder\n");
    return G_SOURCE_CONTINUE;
}

void activate(GtkWidget *button,gpointer data){

    GtkWidget *window = GTK_WIDGET(data);

    printf("activate : %d\n",activated);

    if (activated){
        gtk_window_set_title(GTK_WINDOW(window), "JoytoGear - deactivated");
        g_source_remove(timeout_id);
        timeout_id = 0;
        activated=0;
    }
    else{

        gtk_window_set_title(GTK_WINDOW(window), "JoytoGear - activated");
        init_dev();
        timeout_id = g_timeout_add(mainInterval,  gearboxFF, NULL);
        activated=1;
    }
    
    
}

GtkWidget* init_gtk(int argc, char *argv[]){

    gtk_init(&argc, &argv);
    GtkWidget *boxLeft = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    GtkWidget *boxRight = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

    gtk_window_set_title(GTK_WINDOW(window), "JoytoGear - deactivated");
    gtk_window_set_default_size(GTK_WINDOW(window), 400, 300);

    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    
    

    GtkWidget *combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo), "Aucun");
    gtk_combo_box_set_active(GTK_COMBO_BOX(combo),0);


    GtkWidget *btnActivate = gtk_toggle_button_new_with_label("Activer");
    //gboolean actif = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(btnActivate));

    GtkWidget *Refresh = gtk_button_new_with_label("Refresh Devices");
    //---------------------------------
    //left side
    gtk_box_pack_start(GTK_BOX(boxLeft), Refresh, TRUE, TRUE, 0);
    gtk_box_pack_end(GTK_BOX(boxLeft), combo, TRUE, TRUE, 0);

    //---------------------------------
    //right side
    gtk_box_pack_start(GTK_BOX(boxRight), btnActivate, TRUE, TRUE, 0);

    GtkWidget *boxWindow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_pack_start(GTK_BOX(boxWindow), boxLeft, TRUE, TRUE, 0);
    gtk_box_pack_end(GTK_BOX(boxWindow), boxRight, TRUE, TRUE, 0);

    //if the user change dev , call the ChangePeripheral function
    g_signal_connect(combo, "changed", G_CALLBACK(ChangePeripheral), NULL);
    //refresh dev button
    g_signal_connect(Refresh, "clicked", G_CALLBACK(RefreshPeripheral), combo);
    //
    g_signal_connect(btnActivate, "toggled", G_CALLBACK(activate), window);

    gtk_container_add(GTK_CONTAINER(window), boxWindow);

    RefreshPeripheral(NULL,combo);
    return window;
}



int main(int argc, char *argv[]) {

    GtkWidget *window = init_gtk(argc,argv);

    rcMain =   FindDeviceByName(SelectedDev,&dev);

    if (rc < 0) return -1;
    int fd = libevdev_get_fd(dev);
    if (fd < 0) return -1;

    struct libevdev *vdev = libevdev_new();
    libevdev_set_name(vdev, "virtual-shifter");

    libevdev_enable_event_type(vdev, EV_KEY);
    for (int i = 0; i < 8; i++) {
        libevdev_enable_event_code(vdev, EV_KEY, BTN_TRIGGER_HAPPY1 + i, NULL);
    }

    
    int rc2 = libevdev_uinput_create_from_device(vdev,LIBEVDEV_UINPUT_OPEN_MANAGED,&uidev);
    if (rc2 < 0) {
        fprintf(stderr, "Erreur uinput: %s\n", strerror(-rc));
        return 1;
    }
    libevdev_free(vdev);

    gtk_widget_show_all(window);
    gtk_main();
    return 0;
}

