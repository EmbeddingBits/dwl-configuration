//Modify this file to change what commands output to your statusbar, and recompile using the make command.
static const Block blocks[] = {
    /*Icon*/    /*Command*/                  /*Update Interval*/    /*Update Signal*/
    {"  ", "sh ~/dwl-configuration/scripts/playing.sh",             5,      0},
    {"  ", "cat /sys/class/power_supply/BAT0/capacity | sed 's/$/%/'", 5,      0},
    {"  ", "pamixer --get-volume | sed 's/$/%/'",                   1,      0},
    {"󱑃  ", "date +%H:%M",                                           5,      0},
    {"  ", "free -h | awk '/^Mem/ { print $3 }' | sed s/i//g",      30,     0},
    {"  ", "sh ~/dwl-configuration/scripts/wifi.sh",                5,      0},
    
    /* Updates whenever "pkill -SIGRTMIN+10 someblocks" is ran */
    /* {"", "date '+%b %d (%a) %I:%M%p'",          0,      10}, */
};

//sets delimeter between status commands. NULL character ('\0') means no delimeter.
static char delim[] = " | ";
static unsigned int delimLen = 5;
