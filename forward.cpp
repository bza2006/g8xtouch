#include <fcntl.h>
#include <unistd.h>
#include <linux/uinput.h>
#include <linux/input.h>
#include <cstring>
#include <errno.h>
#include <sys/ioctl.h>
#include <dirent.h>
#include <signal.h>
#include <string>

#define UINPUT_DEV "/dev/uinput"
#define MAX_X 1024
#define MAX_Y 768

static int g_running = 1;

void sig_handler(int sig)
{
    (void)sig;
    g_running = 0;
}

static void emit(int fd, int type, int code, int val)
{
    struct input_event ie{};
    ie.type = type;
    ie.code = code;
    ie.value = val;
    ie.time.tv_sec = 0;
    ie.time.tv_usec = 0;
    write(fd, &ie, sizeof(ie));
}

static std::string find_singwon_tp()
{
    DIR *dir = opendir("/sys/class/input");
    if (!dir) return "";
    struct dirent *ent;
    while ((ent = readdir(dir)) != nullptr)
    {
        std::string dn = ent->d_name;
        if (dn.find("event") != 0) continue;
        std::string namepath = "/sys/class/input/" + dn + "/device/name";
        int fd = open(namepath.c_str(), O_RDONLY);
        if (fd < 0) continue;
        char buf[128]{};
        ssize_t r = read(fd, buf, sizeof(buf)-1);
        close(fd);
        if (r <= 0) continue;
        std::string devname(buf);
        if(devname.find("SingWon-CTP-V1.18B") != std::string::npos)
        {
            closedir(dir);
            return "/dev/input/" + dn;
        }
    }
    closedir(dir);
    return "";
}

static int run_forward(const std::string &input_path)
{
    int fd_in = open(input_path.c_str(), O_RDONLY);
    if (fd_in < 0)
    {
        return -1;
    }

    ioctl(fd_in, EVIOCGRAB, 1);

    int fd_u = open(UINPUT_DEV, O_WRONLY | O_NONBLOCK);
    if (fd_u < 0)
    {
        close(fd_in);
        return -1;
    }

    ioctl(fd_u, UI_SET_EVBIT, EV_KEY);
    ioctl(fd_u, UI_SET_EVBIT, EV_ABS);
    ioctl(fd_u, UI_SET_KEYBIT, BTN_TOUCH);
    ioctl(fd_u, UI_SET_ABSBIT, ABS_MT_SLOT);
    ioctl(fd_u, UI_SET_ABSBIT, ABS_MT_POSITION_X);
    ioctl(fd_u, UI_SET_ABSBIT, ABS_MT_POSITION_Y);
    ioctl(fd_u, UI_SET_ABSBIT, ABS_MT_TRACKING_ID);

    struct uinput_user_dev uidev{};
    snprintf(uidev.name, UINPUT_MAX_NAME_SIZE, "Virtual-Singwon-TP");
    uidev.id.bustype = BUS_USB;
    uidev.id.vendor = 0x222a;
    uidev.id.product = 0x0001;
    uidev.id.version = 1;

    uidev.absmin[ABS_MT_SLOT] = 0;
    uidev.absmax[ABS_MT_SLOT] = 9;
    uidev.absmin[ABS_MT_POSITION_X] = 0;
    uidev.absmax[ABS_MT_POSITION_X] = MAX_X;
    uidev.absmin[ABS_MT_POSITION_Y] = 0;
    uidev.absmax[ABS_MT_POSITION_Y] = MAX_Y;
    uidev.absmin[ABS_MT_TRACKING_ID] = 0;
    uidev.absmax[ABS_MT_TRACKING_ID] = 65535;

    write(fd_u, &uidev, sizeof(uidev));
    ioctl(fd_u, UI_DEV_CREATE);

    struct input_event ev;
    while(g_running)
    {
        ssize_t sz = read(fd_in, &ev, sizeof(ev));
        if(sz != sizeof(ev))
        {
            break;
        }
        write(fd_u, &ev, sizeof(ev));
    }

    ioctl(fd_in, EVIOCGRAB, 0);
    ioctl(fd_u, UI_DEV_DESTROY);
    close(fd_u);
    close(fd_in);
    return 0;
}

int main()
{
    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);

    while(g_running)
    {
        std::string devpath = find_singwon_tp();
        if(!devpath.empty())
        {
            run_forward(devpath);
        }
        sleep(1);
    }
    return 0;
}
