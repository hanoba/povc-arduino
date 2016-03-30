#include "pictures.h"
#include "pic_prost.h"

#include "gif_banana.h"
#include "gif_candle.h"
#include "gif_couple.h"
#include "gif_dog.h"
#include "gif_dog_and_smileys.h"
#include "gif_eye_smiley.h"
#include "gif_groupwave.h"
#include "gif_joke.h"
#include "gif_laughing_smiley.h"
#include "gif_lawn_mower.h"
#include "gif_massbounce.h"
#include "gif_pov.h"
#include "gif_pov_banana.h"
#include "gif_pov_candle.h"
#include "gif_smiley_sport.h"
#include "gif_tantrum.h"
#include "gif_text1.h"
#include "gif_text2.h"
#include "gif_text3.h"
#include "gif_text4.h"
#include "gif_text5.h"
#include "gif_tooth_smiley.h"
#include "gif_twilight.h"
#include "gif_wallbash.h"
#include "gif_xmas.h"

#if 0
#include "gif_dog.h"
#include "gif_rasenmaehen.h"
#include "gif_dog_and_smilies.h"
#include "gif_bananen_barmy.h"
#include "gif_groupwave3.h"
#include "gif_paar3.h"
#include "gif_massbounce3.h"
#include "gif_pov.h"
#include "gif_tantrum.h"
#include "gif_twilight.h"
#endif

const GifFile gifFiles[] PROGMEM = {
#if 0
    { "Dog",            sizeof(gif_dog),             gif_dog,              0, 0 },
    { "Rasenmaehen",    sizeof(gif_rasenmaehen),     gif_rasenmaehen,      0, 0 },
    { "Dog & Smilies",  sizeof(gif_dog_and_smilies), gif_dog_and_smilies,  3, 0 },
    { "Bananen-Barmy",  sizeof(gif_bananen_barmy),   gif_bananen_barmy,    0, 0 },   
    { "Groupwave",      sizeof(gif_groupwave3),      gif_groupwave3,       1, 0 },
    { "Paar",           sizeof(gif_paar3),           gif_paar3,            1, 0 },
    { "Massbounce",     sizeof(gif_massbounce3),     gif_massbounce3,      1, 0 },
    { "POV Cylinder",   sizeof(gif_pov),             gif_pov,              1, 0 },
    { "Tantrum",        sizeof(gif_tantrum),         gif_tantrum,          0, 0 },
    { "Twilight",       sizeof(gif_twilight),        gif_twilight,         0, 0 },
#endif
    { "banana",            sizeof(banana),           banana,            0, 10 },
    { "candle",            sizeof(candle),           candle,            0, 10 },
    { "couple",            sizeof(couple),           couple,            0, 10 },
    { "dog",               sizeof(dog),              dog,               0, 10 },
    { "dog_and_smileys",   sizeof(dog_and_smileys),  dog_and_smileys,   2, 10 },
    { "eye_smiley",        sizeof(eye_smiley),       eye_smiley,        0, 10 },
    { "groupwave",         sizeof(groupwave),        groupwave,         1, 10 },
    { "joke",              sizeof(joke),             joke,              0, 10 },
    { "lawn_mower",        sizeof(lawn_mower),       lawn_mower,        0, 10 },
    { "laughing_smiley",   sizeof(laughing_smiley),  laughing_smiley,   0, 10 },
    { "massbounce",        sizeof(massbounce),       massbounce,        1, 10 },
    { "pov",               sizeof(pov),              pov,               0, 10 },
    { "pov_banana",        sizeof(pov_banana),       pov_banana,        0, 10 },
    { "pov_candle",        sizeof(pov_candle),       pov_candle,        0, 10 },
    { "smiley_sport",      sizeof(smiley_sport),     smiley_sport,      0, 10 },
    { "tantrum",           sizeof(tantrum),          tantrum,           0, 10 },
    { "text1",             sizeof(text1),            text1,             1, 10 },
    { "text2",             sizeof(text2),            text2,             1, 10 },
    { "text3",             sizeof(text3),            text3,             1, 10 },
    { "text4",             sizeof(text4),            text4,             1, 10 },
    { "text5",             sizeof(text5),            text5,             1, 10 },
    { "tooth_smiley",      sizeof(tooth_smiley),     tooth_smiley,      0, 10 },
    { "twilight",          sizeof(twilight),         twilight,          0, 10 },
    { "wallbash",          sizeof(wallbash),         wallbash,          0, 10 },
    { "xmas",              sizeof(xmas),             xmas,              0, 10 },    
    { "",                  0,                        0,                 0, 0  }
};
