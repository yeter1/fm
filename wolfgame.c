/*
 * wolfgame in c
 * 
 * this Jeremy Clarkson idea was brought to you by eta
 */
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <stdio.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <assert.h>
#include "varlibs/vbuf.h"
#define WGP_ENUMERATE(wg) DPA_ENUMERATE(wg->players)
#define WGP_ENUMERATE_DEAD(wg) DPA_ENUMERATE(wg->dead)
#define KCHOICE_ENUMERATE(wg) DPA_ENUMERATE(wg->kchoices)
#define AMSG(args) wgp_msg_sprintf(NULL, args)
#define EWGP ((struct wg_player *) wolfgame->players->keys[DPA_N])
#define EDWGP ((struct wg_player *) wolfgame->dead->keys[DPA_N])
#define EKCHOICE ((struct wg_kchoice *) wolfgame->kchoices->keys[DPA_N])
char *ONE = "one";
char *TWO = "two";
char *THREE = "three";
char *FOUR = "four";
enum wg_roles { VILLAGER, WOLF, SEER, NONE };
enum wg_roles conf_4p[2] = { SEER, WOLF };
enum wg_states { DAY, NIGHT };
struct wg_player {
    enum wg_roles role;
    char *id;
    int votes;
    bool free_my_id;
    bool acted;
    bool dead;
};
struct wg_kchoice {
    struct wg_player *actor;
    struct wg_player *tgt;
};
struct wolfgame {
    DPA *players;
    DPA *dead;
    DPA *kchoices;
    enum wg_states state;
};
struct wolfgame *wolfgame = NULL;
void wg_log(char *fmt, ...) {
    va_list va;
    va_start(va, fmt);
    vfprintf(stderr, fmt, va);
    va_end(va);
}
struct wg_player *wgp_init(void) {
    struct wg_player *pl = malloc(sizeof(struct wg_player));
    assert(pl != NULL);
    pl->role = NONE;
    pl->id = NULL;
    pl->acted = false;
    pl->dead = false;
    pl->free_my_id = false;
}
void wgp_msg(struct wg_player *wgp, char *msg) {
    printf("MSG %s: %s\n", wgp->id, msg);
}
void wg_amsg(char *msg) {
    printf("AMSG %s\n", msg);
}
void wgp_msg_sprintf(struct wg_player *wgp, const char *format, ...) {
    va_list vali, valn;
    va_start(vali, format);
    va_copy(valn, vali);
    int len = 2; /* extra bytes, for safety */
    len += vsnprintf(NULL, 0, format, vali);
    char buf[len];
    vsnprintf(buf, len, format, valn);
    if (wgp != NULL) wgp_msg(wgp, buf);
    else wg_amsg(buf);
    va_end(vali);
    va_end(valn);
}
void wgp_msg_avail_players(struct wg_player *wgp) {
    int namelen = 10;
    namelen += strlen("Available players: ");
    WGP_ENUMERATE(wolfgame) {
        namelen += strlen(EWGP->id);
        namelen += 1;
    }
    char str[namelen];
    memset(&str, 0, namelen);
    strcat(str, "Available players: ");
    WGP_ENUMERATE(wolfgame) {
        strcat(str, EWGP->id);
        strcat(str, " ");
    }
    wgp_msg(wgp, str);
}
struct wg_player *wg_target(char *id) {
    WGP_ENUMERATE(wolfgame) {
        if (strcmp(EWGP->id, id) == 0) {
            return EWGP;
        }
    }
    return NULL;
}
bool wg_check_day(void) {
    bool ready = true;
    WGP_ENUMERATE(wolfgame) {
        if (EWGP->role != VILLAGER && EWGP->acted == false) ready = false;
    }
    return ready;
}
const char *wg_rtc(enum wg_roles role) {
    switch (role) {
        case SEER:
            return "seer";
            break;
        case WOLF:
            return "wolf";
            break;
        case VILLAGER:
            return "villager";
            break;
        default:
            return "glitch";
            break;
    }
}
void wg_kchoice_add(struct wg_player *actor, struct wg_player *tgt) {
    if (actor == tgt && wolfgame->state != DAY) {
        wgp_msg(actor, "Suicide is bad! Don't do it.");
        return;
    }
    struct wg_kchoice *wgk = malloc(sizeof(struct wg_kchoice));
    assert(wgk != NULL);
    KCHOICE_ENUMERATE(wolfgame) {
        if (EKCHOICE->actor == actor) {
            DPA_rem(wolfgame->kchoices, EKCHOICE); /* make sure one player
                                                      doesn't register 2 kchoices */
            break;
        }
    }
    wgk->actor = actor;
    wgk->tgt = tgt;
    DPA_store(wolfgame->kchoices, wgk);
    if (wolfgame->state == NIGHT) {
        actor->acted = true;
        wgp_msg_sprintf(actor, "You select %s to be killed tonight.", tgt->id);
    }
    else {
        wgp_msg_sprintf(NULL, "%s votes against %s.", actor->id, tgt->id);
    }
}
void wg_role_act(struct wg_player *actr, struct wg_player *tgt) {
    switch (actr->role) {
        case SEER:
            if (actr->acted) return wgp_msg(actr, "You may not see more than one person per night.");
            wgp_msg_sprintf(actr, "You, through your magical powers, divine %s to be a %s!", tgt->id, wg_rtc(tgt->role));
            actr->acted = true;
            break;
        case WOLF:
            wg_kchoice_add(actr, tgt);
            break;
        default:
            wgp_msg_sprintf(actr, "You do not have a special ability.");
            break;
    }
}
void wgp_night(struct wg_player *wgp) {
    switch (wgp->role) {
        case SEER:
            wgp_msg(wgp, "You are a seer.");
            wgp_msg(wgp, "You may choose one person to see each night.");
            wgp_msg_avail_players(wgp);
            wgp->acted = false;
            break;
        case WOLF:
            wgp_msg(wgp, "You are a wolf!");
            wgp_msg(wgp, "You may choose one person to kill each night.");
            wgp_msg_avail_players(wgp);
            wgp->acted = false;
            break;
        default:
            break;
    }
}
void wg_kchoice_cleanslate(void) {
    DPA *dpa = wolfgame->kchoices;
    DPA_ENUMERATE(dpa) {
        free(dpa->keys[DPA_N]);
    }
    DPA_free(dpa);
    wolfgame->kchoices = DPA_init();
    assert(wolfgame->kchoices != NULL);
}
struct wg_player *wg_kchoice_analyse(bool majority) {
    struct wg_player *wgp = NULL;
    int mvotes = 0;
    WGP_ENUMERATE(wolfgame) {
        EWGP->votes = 0;
        wgp = EWGP; /* DPA_N will get lost inside KCHOICE_ENUMERATE */
        KCHOICE_ENUMERATE(wolfgame) {
            if (EKCHOICE->tgt == wgp) {
                (wgp->votes)++; /* there's some weird rule about ++ and pointers, better
                                                         to play it safe */
            }
            if (wgp->votes > mvotes && !majority) mvotes = wgp->votes;
        }
    }
    if (mvotes == 0) return NULL;
    if (majority) mvotes = (wolfgame->players->used / 2) + 1;
    WGP_ENUMERATE(wolfgame) {
        if (EWGP->votes >= mvotes) {
            wg_kchoice_cleanslate();
            return EWGP;
        }
    }
    return NULL;
}
void wg_gameover(void) {
    wgp_msg_sprintf(NULL, "Game over!");
    WGP_ENUMERATE(wolfgame) {
        wgp_msg_sprintf(NULL, "%s survived! They were a %s.", EWGP->id, wg_rtc(EWGP->role));
    }
    WGP_ENUMERATE_DEAD(wolfgame) {
        wgp_msg_sprintf(NULL, "%s died. They were a %s.", EDWGP->id, wg_rtc(EDWGP->role));
    }
    wgp_msg_sprintf(NULL, "Thanks for playing! This game was brought to you by eeeeeta.");
    exit(0);
}
void wg_check_endgame(void) {
    int villagers = 0;
    int wolves = 0;
    WGP_ENUMERATE(wolfgame) {
        switch (EWGP->role) {
            case WOLF:
                wolves++;
                break;
            case SEER:
                villagers++;
                break;
            case VILLAGER:
                villagers++;
                break;
            default:
                /* welp. */
                assert((2+2) != 4);
                break;
        }
    }
    if (wolves >= villagers) {
        /* outnumbered! */
        wgp_msg_sprintf(NULL, "The wolves outnumber the villagers and eat them all!"); /* oh no :( */
        wg_gameover();
    }
    if (wolves == 0) {
        wgp_msg_sprintf(NULL, "All the wolves are dead! The villagers chop them up, BBQ them, and have a nice dinner.");
        wg_gameover();
    }
}
void wg_kill_player(struct wg_player *wgp) {
    /* Poor player :( */
    wgp->dead = true;
    DPA_rem(wolfgame->players, wgp);
    DPA_store(wolfgame->dead, wgp);
    wg_check_endgame();
}
bool wg_check_lynches(void) {
    struct wg_player *wgp = wg_kchoice_analyse(true);
    if (!wgp) return false;
    else {
        wgp_msg_sprintf(NULL, "The villagers decide to lynch %s, a %s, by majority vote.", wgp->id, wg_rtc(wgp->role));
        wg_kill_player(wgp);
        return true;
    }
}
void wg_night(void);
void wg_day(void) {
    wg_log("[+] It is now day.\n");
    printf("STATE DAY\n");
    wolfgame->state = DAY;
    struct wg_player *wgp = wg_kchoice_analyse(false);
    wgp_msg_sprintf(NULL, "The sun rises. The villagers, tired from the night before, get up and search the village...");
    if (wgp == NULL) wgp_msg_sprintf(NULL, "Traces of wolf blood and fur are found near the city hall. However, no casualties are present.");
    else {
        wgp_msg_sprintf(NULL, "The corpse of %s is found. After further analysis, it is revealed that they were a %s.", wgp->id, wg_rtc(wgp->role));
        wg_kill_player(wgp);
    }
    wgp_msg_sprintf(NULL, "The villagers must now decide who to lynch. A majority vote is required; an even split or no votes will not result in a lynching.");
    printf("ROLEINPUT\n");
    unsigned long millis = 0UL;
    while (millis < 120000UL) {
        struct pollfd pfds[1];
        pfds[0].fd = fileno(stdin);
        pfds[0].events = POLLIN;
        int pollrv = -1;
        pollrv = poll(pfds, 1, 10000);
        if (pollrv == -1) {
            wg_log("[-] Error in poll() :(\n");
            perror("poll()");
            assert(pollrv != -1);
        }
        if (pollrv != 0 && pfds[0].revents & POLLIN) {
            char id[50] = {0};
            char act[50] = {0};
            struct wg_player *actor = NULL;
            struct wg_player *tgt = NULL;
            scanf("%s %s", id, act);
            if ((actor = wg_target(id)) != NULL) {
                if ((tgt = wg_target(act)) == NULL) {
                    printf("INVALIDTARGET\n");
                }
                else {
                    wg_log("[+] Executing action: %s lynches %s\n", actor->id, tgt->id);
                    wg_kchoice_add(actor, tgt);
                    bool ready = wg_check_lynches();
                    if (ready) {
                        wg_log("[+] Check returned true, starting night transition!\n");
                        break;
                    }
                }
            }
            else {
                printf("INVALIDPLAYER\n");
            }
        }
        millis += 10000UL;
        if (millis > 100000UL) printf("WTIMEOUT\n");
        if (millis > 120000UL) printf("ETIMEOUT\n");
    }
    wg_night();
}
void wg_night(void) {
    wg_log("[+] It is now night.\n");
    printf("STATE NIGHT\n");
    wgp_msg_sprintf(NULL, "It is now night.");
    wolfgame->state = NIGHT;
    WGP_ENUMERATE(wolfgame) {
        wgp_night(EWGP);
    }
    printf("ROLEINPUT\n");
    unsigned long millis = 0UL;
    return wg_day();
    while (millis < 120000UL) {
        struct pollfd pfds[1];
        pfds[0].fd = fileno(stdin);
        pfds[0].events = POLLIN;
        int pollrv = -1;
        pollrv = poll(pfds, 1, 10000);
        if (pollrv == -1) {
            perror("poll()");
            assert(pollrv != -1);
        }
        if (pollrv != 0 && pfds[0].revents & POLLIN) {
            char id[50] = {0};
            char act[50] = {0};
            struct wg_player *actor = NULL;
            struct wg_player *tgt = NULL;
            scanf("%s %s", id, act);
            if ((actor = wg_target(id)) != NULL) {
                if ((tgt = wg_target(act)) == NULL) {
                    printf("INVALIDTARGET\n");
                }
                else {
                    printf("[+] Executing action: %s (%s) acts upon %s (%s)\n", actor->id, wg_rtc(actor->role), tgt->id, wg_rtc(tgt->role));
                    wg_role_act(actor, tgt);
                    bool ready = wg_check_day();
                    if (ready) {
                        printf("[+] Check returned true, starting day transition!\n");
                        break;
                    }
                }
            }
            else {
                printf("INVALIDPLAYER");
            }
        }
        millis += 10000UL;
        if (millis > 100000UL) printf("night close to timeout\n");
        if (millis > 120000UL) printf("night timed out\n");
    }
    wg_day();
}
void role_chooser() {
    WGP_ENUMERATE(wolfgame) {
        enum wg_roles chosen = VILLAGER;
        if (DPA_N == 0) chosen = WOLF; /* much random */
        if (DPA_N == 1) chosen = SEER; /* very sekrit */
        EWGP->role = chosen;
    };
}
int main(int argc, char *argv[]) {
    wg_log("Ultimate Wolfgame Engine, v0.0.1\n");
    wg_log("a silly thing, by eta\n");
    wg_log("[+] Initialising memory structures..\n");
    wolfgame = malloc(sizeof(struct wolfgame));
    assert(wolfgame != NULL);
    wolfgame->players = DPA_init();
    wolfgame->kchoices = DPA_init();
    wolfgame->dead = DPA_init();
    assert(wolfgame->players != NULL && wolfgame->kchoices != NULL && wolfgame->dead != NULL);
    wg_log("[+] Using example game\n");
    for (int x = 0; x < 4; x++) {
        struct wg_player *wgp = DPA_store(wolfgame->players, wgp_init());
        assert(wgp != NULL);
        switch (x) {
            case 0:
                wgp->id = ONE;
                break;
            case 1:
                wgp->id = TWO;
                break;
            case 2:
                wgp->id = THREE;
                break;
            case 3:
                wgp->id = FOUR;
                break;
            default:
                break;
        }
    }
    role_chooser();
    wg_night();
};
