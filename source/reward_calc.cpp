#include "reward_calc.h"
#include "xoroshiro128plus.h"
#include <cstdio>
#include <cstring>

bool RewardCalc::loadTables(const std::string& fixedPath, const std::string& lotteryPath) {
    // Load fixed reward tables
    {
        FILE* f = fopen(fixedPath.c_str(), "rb");
        if (!f) return false;
        fseek(f, 0, SEEK_END);
        long size = ftell(f);
        fseek(f, 0, SEEK_SET);
        std::vector<uint8_t> data(size);
        fread(data.data(), 1, size, f);
        fclose(f);

        auto r16 = [](const uint8_t* p) -> uint16_t { return p[0] | (p[1] << 8); };
        auto r64 = [](const uint8_t* p) -> uint64_t {
            uint64_t v = 0;
            for (int i = 0; i < 8; i++) v |= (uint64_t)p[i] << (i * 8);
            return v;
        };

        const uint8_t* ptr = data.data();
        uint16_t tableCount = r16(ptr); ptr += 2;

        for (int t = 0; t < tableCount; t++) {
            uint64_t hash = r64(ptr); ptr += 8;
            uint8_t count = *ptr++;
            std::vector<FixedEntry> items;
            for (int i = 0; i < count; i++) {
                FixedEntry e;
                e.category = *ptr++;
                e.itemId = r16(ptr); ptr += 2;
                e.amount = *ptr++;
                e.subjectType = (int8_t)*ptr++;
                items.push_back(e);
            }
            fixedTables_[hash] = std::move(items);
        }
    }

    // Load lottery reward tables
    {
        FILE* f = fopen(lotteryPath.c_str(), "rb");
        if (!f) return false;
        fseek(f, 0, SEEK_END);
        long size = ftell(f);
        fseek(f, 0, SEEK_SET);
        std::vector<uint8_t> data(size);
        fread(data.data(), 1, size, f);
        fclose(f);

        auto r16 = [](const uint8_t* p) -> uint16_t { return p[0] | (p[1] << 8); };
        auto r64 = [](const uint8_t* p) -> uint64_t {
            uint64_t v = 0;
            for (int i = 0; i < 8; i++) v |= (uint64_t)p[i] << (i * 8);
            return v;
        };

        const uint8_t* ptr = data.data();
        uint16_t tableCount = r16(ptr); ptr += 2;

        for (int t = 0; t < tableCount; t++) {
            uint64_t hash = r64(ptr); ptr += 8;
            uint16_t totalRate = r16(ptr); ptr += 2;
            uint8_t count = *ptr++;
            LotteryTable lt;
            lt.totalRate = totalRate;
            for (int i = 0; i < count; i++) {
                LotteryEntry e;
                e.category = *ptr++;
                e.itemId = r16(ptr); ptr += 2;
                e.amount = *ptr++;
                e.rate = r16(ptr); ptr += 2;
                lt.items.push_back(e);
            }
            lotteryTables_[hash] = std::move(lt);
        }
    }

    return true;
}

// Reward roll count by star rating (from RewardUtil.cs)
static constexpr int REWARD_SLOTS[7][5] = {
    {4, 5, 6, 7, 8},   // 1-star
    {4, 5, 6, 7, 8},   // 2-star
    {5, 6, 7, 8, 9},   // 3-star
    {5, 6, 7, 8, 9},   // 4-star
    {6, 7, 8, 9, 10},  // 5-star
    {7, 8, 9, 10, 11}, // 6-star
    {7, 8, 9, 10, 11}, // 7-star (mighty)
};

int RewardCalc::getRewardCount(uint64_t random, int stars) {
    int idx = stars - 1;
    if (idx < 0) idx = 0;
    if (idx > 6) idx = 6;
    if (random < 10) return REWARD_SLOTS[idx][0];
    if (random < 40) return REWARD_SLOTS[idx][1];
    if (random < 70) return REWARD_SLOTS[idx][2];
    if (random < 90) return REWARD_SLOTS[idx][3];
    return REWARD_SLOTS[idx][4];
}

uint16_t RewardCalc::getTeraShardId(uint8_t teraType) {
    // Type order: Normal=0, Fighting=1, Flying=2, Poison=3, Ground=4,
    //             Rock=5, Bug=6, Ghost=7, Steel=8, Fire=9, Water=10,
    //             Grass=11, Electric=12, Psychic=13, Ice=14, Dragon=15,
    //             Dark=16, Fairy=17
    static const uint16_t shardIds[] = {
        1862, // Normal
        1868, // Fighting
        1871, // Flying
        1869, // Poison
        1870, // Ground
        1874, // Rock
        1873, // Bug
        1875, // Ghost
        1878, // Steel
        1863, // Fire
        1864, // Water
        1866, // Grass
        1865, // Electric
        1872, // Psychic
        1867, // Ice
        1876, // Dragon
        1877, // Dark
        1879, // Fairy
    };
    if (teraType < 18) return shardIds[teraType];
    return 1862;
}

// Species -> Pokemon Material ID mapping (from RewardUtil.cs GetMaterial())
uint16_t RewardCalc::getMaterialId(uint16_t species) {
    switch (species) {
        case 48: case 49: return 1956;    // Venonat, Venomoth
        case 50: case 51: return 1957;    // Diglett, Dugtrio
        case 52: case 53: return 1958;    // Meowth, Persian
        case 54: case 55: return 1959;    // Psyduck, Golduck
        case 56: case 57: case 979: return 1960; // Mankey, Primeape, Annihilape
        case 58: case 59: return 1961;    // Growlithe, Arcanine
        case 79: case 80: case 199: return 1962; // Slowpoke, Slowbro, Slowking
        case 81: case 82: case 462: return 1963; // Magnemite, Magneton, Magnezone
        case 88: case 89: return 1964;    // Grimer, Muk
        case 90: case 91: return 1965;    // Shellder, Cloyster
        case 92: case 93: case 94: return 1966; // Gastly, Haunter, Gengar
        case 96: case 97: return 1967;    // Drowzee, Hypno
        case 100: case 101: return 1968;  // Voltorb, Electrode
        case 123: case 212: case 900: return 1969; // Scyther, Scizor, Kleavor
        case 128: return 1970;            // Tauros
        case 129: case 130: return 1971;  // Magikarp, Gyarados
        case 132: return 1972;            // Ditto
        case 133: case 134: case 135: case 136: case 196: case 197:
        case 470: case 471: case 700: return 1973; // Eevee family
        case 147: case 148: case 149: return 1974; // Dratini, Dragonair, Dragonite
        case 172: case 25: case 26: return 1975;   // Pichu, Pikachu, Raichu
        case 174: case 39: case 40: return 1976;    // Igglybuff, Jigglypuff, Wigglytuff
        case 179: case 180: case 181: return 1977;  // Mareep, Flaaffy, Ampharos
        case 187: case 188: case 189: return 1978;  // Hoppip, Skiploom, Jumpluff
        case 191: case 192: return 1979;  // Sunkern, Sunflora
        case 198: case 430: return 1980;  // Murkrow, Honchkrow
        case 200: case 429: return 1981;  // Misdreavus, Mismagius
        case 203: case 981: return 1982;  // Girafarig, Farigiraf
        case 204: case 205: return 1983;  // Pineco, Forretress
        case 206: case 982: return 1984;  // Dunsparce, Dudunsparce
        case 211: case 904: return 1985;  // Qwilfish, Overqwil
        case 214: return 1986;            // Heracross
        case 215: case 461: case 903: return 1987; // Sneasel, Weavile, Sneasler
        case 216: case 217: case 901: return 1988; // Teddiursa, Ursaring, Ursaluna
        case 225: return 1989;            // Delibird
        case 228: case 229: return 1990;  // Houndour, Houndoom
        case 231: case 232: return 1991;  // Phanpy, Donphan
        case 234: case 899: return 1992;  // Stantler, Wyrdeer
        case 246: case 247: case 248: return 1993; // Larvitar, Pupitar, Tyranitar
        case 278: case 279: return 1994;  // Wingull, Pelipper
        case 280: case 281: case 282: case 475: return 1995; // Ralts family
        case 283: case 284: return 1996;  // Surskit, Masquerain
        case 285: case 286: return 1997;  // Shroomish, Breloom
        case 287: case 288: case 289: return 1998; // Slakoth, Vigoroth, Slaking
        case 296: case 297: return 1999;  // Makuhita, Hariyama
        case 298: case 183: case 184: return 2000; // Azurill, Marill, Azumarill
        case 302: return 2001;            // Sableye
        case 307: case 308: return 2002;  // Meditite, Medicham
        case 316: case 317: return 2003;  // Gulpin, Swalot
        case 322: case 323: return 2004;  // Numel, Camerupt
        case 324: return 2005;            // Torkoal
        case 325: case 326: return 2006;  // Spoink, Grumpig
        case 331: case 332: return 2007;  // Cacnea, Cacturne
        case 333: case 334: return 2008;  // Swablu, Altaria
        case 335: return 2009;            // Zangoose
        case 336: return 2010;            // Seviper
        case 339: case 340: return 2011;  // Barboach, Whiscash
        case 353: case 354: return 2012;  // Shuppet, Banette
        case 357: return 2013;            // Tropius
        case 361: case 362: case 478: return 2014; // Snorunt, Glalie, Froslass
        case 370: return 2015;            // Luvdisc
        case 371: case 372: case 373: return 2016; // Bagon, Shelgon, Salamence
        case 396: case 397: case 398: return 2017; // Starly, Staravia, Staraptor
        case 401: case 402: return 2018;  // Kricketot, Kricketune
        case 403: case 404: case 405: return 2019; // Shinx, Luxio, Luxray
        case 415: case 416: return 2020;  // Combee, Vespiquen
        case 417: return 2021;            // Pachirisu
        case 418: case 419: return 2022;  // Buizel, Floatzel
        case 422: case 423: return 2023;  // Shellos, Gastrodon
        case 425: case 426: return 2024;  // Drifloon, Drifblim
        case 434: case 435: return 2025;  // Stunky, Skuntank
        case 436: case 437: return 2026;  // Bronzor, Bronzong
        case 438: case 185: return 2027;  // Bonsly, Sudowoodo
        case 440: case 113: case 242: return 2028; // Happiny, Chansey, Blissey
        case 442: return 2029;            // Spiritomb
        case 443: case 444: case 445: return 2030; // Gible, Gabite, Garchomp
        case 447: case 448: return 2031;  // Riolu, Lucario
        case 449: case 450: return 2032;  // Hippopotas, Hippowdon
        case 453: case 454: return 2033;  // Croagunk, Toxicroak
        case 456: case 457: return 2034;  // Finneon, Lumineon
        case 459: case 460: return 2035;  // Snover, Abomasnow
        case 479: return 2036;            // Rotom
        case 548: case 549: return 2037;  // Petilil, Lilligant
        case 550: case 902: return 2038;  // Basculin, Basculegion
        case 551: case 552: case 553: return 2039; // Sandile, Krokorok, Krookodile
        case 570: case 571: return 2040;  // Zorua, Zoroark
        case 574: case 575: case 576: return 2041; // Gothita, Gothorita, Gothitelle
        case 585: case 586: return 2042;  // Deerling, Sawsbuck
        case 590: case 591: return 2043;  // Foongus, Amoonguss
        case 594: return 2044;            // Alomomola
        case 602: case 603: case 604: return 2045; // Tynamo, Eelektrik, Eelektross
        case 610: case 611: case 612: return 2046; // Axew, Fraxure, Haxorus
        case 613: case 614: return 2047;  // Cubchoo, Beartic
        case 615: return 2048;            // Cryogonal
        case 624: case 625: case 983: return 2049; // Pawniard, Bisharp, Kingambit
        case 627: case 628: return 2050;  // Rufflet, Braviary
        case 633: case 634: case 635: return 2051; // Deino, Zweilous, Hydreigon
        case 636: case 637: return 2052;  // Larvesta, Volcarona
        case 661: case 662: case 663: return 2053; // Fletchling, Fletchinder, Talonflame
        case 664: case 665: case 666: return 2054; // Scatterbug, Spewpa, Vivillon
        case 667: case 668: return 2055;  // Litleo, Pyroar
        case 669: case 670: case 671: return 2056; // Flabebe, Floette, Florges
        case 672: case 673: return 2057;  // Skiddo, Gogoat
        case 690: case 691: return 2058;  // Skrelp, Dragalge
        case 692: case 693: return 2059;  // Clauncher, Clawitzer
        case 701: return 2060;            // Hawlucha
        case 702: return 2061;            // Dedenne
        case 704: case 705: case 706: return 2062; // Goomy, Sliggoo, Goodra
        case 707: return 2063;            // Klefki
        case 712: case 713: return 2064;  // Bergmite, Avalugg
        case 714: case 715: return 2065;  // Noibat, Noivern
        case 734: case 735: return 2066;  // Yungoos, Gumshoos
        case 739: case 740: return 2067;  // Crabrawler, Crabominable
        case 741: return 2068;            // Oricorio
        case 744: case 745: return 2069;  // Rockruff, Lycanroc
        case 747: case 748: return 2070;  // Mareanie, Toxapex
        case 749: case 750: return 2071;  // Mudbray, Mudsdale
        case 753: case 754: return 2072;  // Fomantis, Lurantis
        case 757: case 758: return 2073;  // Salandit, Salazzle
        case 761: case 762: case 763: return 2074; // Bounsweet, Steenee, Tsareena
        case 765: return 2075;            // Oranguru
        case 766: return 2076;            // Passimian
        case 769: case 770: return 2077;  // Sandygast, Palossand
        case 775: return 2078;            // Komala
        case 778: return 2079;            // Mimikyu
        case 779: return 2080;            // Bruxish
        case 833: case 834: return 2081;  // Chewtle, Drednaw
        case 819: case 820: return 2082;  // Skwovet, Greedent
        case 846: case 847: return 2083;  // Arrokuda, Barraskewda
        case 821: case 822: case 823: return 2084; // Rookidee, Corvisquire, Corviknight
        case 848: case 849: return 2085;  // Toxel, Toxtricity
        case 870: return 2086;            // Falinks
        case 878: case 879: return 2087;  // Cufant, Copperajah
        case 837: case 838: case 839: return 2088; // Rolycoly, Carkol, Coalossal
        case 843: case 844: return 2089;  // Silicobra, Sandaconda
        case 876: return 2090;            // Indeedee
        case 871: return 2091;            // Pincurchin
        case 872: case 873: return 2092;  // Snom, Frosmoth
        case 859: case 860: case 861: return 2093; // Impidimp, Morgrem, Grimmsnarl
        case 840: case 841: case 842: case 1011: return 2094; // Applin, Flapple, Appletun, Dipplin
        case 854: case 855: return 2095;  // Sinistea, Polteageist
        case 856: case 857: case 858: return 2096; // Hatenna, Hattrem, Hatterene
        case 874: return 2097;            // Stonjourner
        case 875: return 2098;            // Eiscue
        case 885: case 886: case 887: return 2099; // Dreepy, Drakloak, Dragapult
        case 915: case 916: return 2103;  // Lechonk, Oinkologne
        case 917: case 918: return 2104;  // Tarountula, Spidops
        case 919: case 920: return 2105;  // Nymble, Lokix
        case 953: case 954: return 2106;  // Rellor, Rabsca
        case 971: case 972: return 2107;  // Greavard, Houndstone
        case 955: case 956: return 2108;  // Flittle, Espathra
        case 960: case 961: return 2109;  // Wiglett, Wugtrio
        case 978: return 2110;            // Dondozo
        case 976: return 2111;            // Veluza
        case 963: case 964: return 2112;  // Finizen, Palafin
        case 928: case 929: case 930: return 2113; // Smoliv, Dolliv, Arboliva
        case 951: case 952: return 2114;  // Capsakid, Scovillain
        case 938: case 939: return 2115;  // Tadbulb, Bellibolt
        case 965: case 966: return 2116;  // Varoom, Revavroom
        case 968: return 2117;            // Orthworm
        case 924: case 925: return 2118;  // Tandemaus, Maushold
        case 974: case 975: return 2119;  // Cetoddle, Cetitan
        case 996: case 997: case 998: return 2120; // Frigibax, Arctibax, Baxcalibur
        case 977: return 2121;            // Tatsugiri
        case 967: return 2122;            // Cyclizar
        case 921: case 922: case 923: return 2123; // Pawmi, Pawmo, Pawmot
        case 940: case 941: return 2126;  // Wattrel, Kilowattrel
        case 962: return 2127;            // Bombirdier
        case 931: return 2128;            // Squawkabilly
        case 973: return 2129;            // Flamigo
        case 950: return 2130;            // Klawf
        case 932: case 933: case 934: return 2131; // Nacli, Naclstack, Garganacl
        case 969: case 970: return 2132;  // Glimmet, Glimmora
        case 944: case 945: return 2133;  // Shroodle, Grafaiai
        case 926: case 927: return 2134;  // Fidough, Dachsbun
        case 942: case 943: return 2135;  // Maschiff, Mabosstiff
        case 946: case 947: return 2136;  // Bramblin, Brambleghast
        case 999: case 1000: return 2137; // Gimmighoul, Gholdengo
        case 957: case 958: case 959: return 2156; // Tinkatink, Tinkatuff, Tinkaton
        case 935: case 936: case 937: return 2157; // Charcadet, Armarouge, Ceruledge
        case 948: case 949: return 2158;  // Toedscool, Toedscruel
        case 194: case 195: case 980: return 2159; // Wooper, Quagsire, Clodsire
        // DLC species
        case 23: case 24: return 2438;    // Ekans, Arbok
        case 27: case 28: return 2439;    // Sandshrew, Sandslash
        case 173: case 35: case 36: return 2440; // Cleffa, Clefairy, Clefable
        case 37: case 38: return 2441;    // Vulpix, Ninetales
        case 60: case 61: case 62: case 186: return 2442; // Poliwag family
        case 69: case 70: case 71: return 2443; // Bellsprout, Weepinbell, Victreebel
        case 74: case 75: case 76: return 2444; // Geodude, Graveler, Golem
        case 109: case 110: return 2445;  // Koffing, Weezing
        case 446: case 143: return 2446;  // Munchlax, Snorlax
        case 161: case 162: return 2447;  // Sentret, Furret
        case 163: case 164: return 2448;  // Hoothoot, Noctowl
        case 167: case 168: return 2449;  // Spinarak, Ariados
        case 190: case 424: return 2450;  // Aipom, Ambipom
        case 193: case 469: return 2451;  // Yanma, Yanmega
        case 207: case 472: return 2452;  // Gligar, Gliscor
        case 218: case 219: return 2453;  // Slugma, Magcargo
        case 220: case 221: case 473: return 2454; // Swinub, Piloswine, Mamoswine
        case 261: case 262: return 2455;  // Poochyena, Mightyena
        case 270: case 271: case 272: return 2456; // Lotad, Lombre, Ludicolo
        case 273: case 274: case 275: return 2457; // Seedot, Nuzleaf, Shiftry
        case 299: case 476: return 2458;  // Nosepass, Probopass
        case 313: return 2459;            // Volbeat
        case 314: return 2460;            // Illumise
        case 341: case 342: return 2461;  // Corphish, Crawdaunt
        case 349: case 350: return 2462;  // Feebas, Milotic
        case 355: case 356: case 477: return 2463; // Duskull, Dusclops, Dusknoir
        case 358: case 433: return 2464;  // Chimecho, Chingling
        case 532: case 533: case 534: return 2465; // Timburr, Gurdurr, Conkeldurr
        case 540: case 541: case 542: return 2466; // Sewaddle, Swadloon, Leavanny
        case 580: case 581: return 2467;  // Ducklett, Swanna
        case 607: case 608: case 609: return 2468; // Litwick, Lampent, Chandelure
        case 619: case 620: return 2469;  // Mienfoo, Mienshao
        case 629: case 630: return 2470;  // Vullaby, Mandibuzz
        case 703: return 2471;            // Carbink
        case 708: case 709: return 2472;  // Phantump, Trevenant
        case 736: case 737: case 738: return 2473; // Grubbin, Charjabug, Vikavolt
        case 742: case 743: return 2474;  // Cutiefly, Ribombee
        case 782: case 783: case 784: return 2475; // Jangmo-o, Hakamo-o, Kommo-o
        case 845: return 2476;            // Cramorant
        case 877: return 2477;            // Morpeko
        case 1012: case 1013: return 2478; // Poltchageist, Sinistcha
        case 43: case 44: case 45: case 182: return 2484; // Oddish family
        case 72: case 73: return 2485;    // Tentacool, Tentacruel
        case 84: case 85: return 2486;    // Doduo, Dodrio
        case 86: case 87: return 2487;    // Seel, Dewgong
        case 102: case 103: return 2488;  // Exeggcute, Exeggutor
        case 236: case 106: case 107: case 237: return 2489; // Tyrogue, Hitmonlee, Hitmonchan, Hitmontop
        case 111: case 112: case 464: return 2490; // Rhyhorn, Rhydon, Rhyperior
        case 116: case 117: case 230: return 2491; // Horsea, Seadra, Kingdra
        case 239: case 125: case 466: return 2492; // Elekid, Electabuzz, Electivire
        case 240: case 126: case 467: return 2493; // Magby, Magmar, Magmortar
        case 131: return 2494;            // Lapras
        case 137: case 233: case 474: return 2495; // Porygon, Porygon2, Porygon-Z
        case 170: case 171: return 2496;  // Chinchou, Lanturn
        case 209: case 210: return 2497;  // Snubbull, Granbull
        case 227: return 2498;            // Skarmory
        case 235: return 2499;            // Smeargle
        case 311: return 2500;            // Plusle
        case 312: return 2501;            // Minun
        case 328: case 329: case 330: return 2502; // Trapinch, Vibrava, Flygon
        case 374: case 375: case 376: return 2503; // Beldum, Metang, Metagross
        case 408: case 409: return 2504;  // Cranidos, Rampardos
        case 410: case 411: return 2505;  // Shieldon, Bastiodon
        case 522: case 523: return 2506;  // Blitzle, Zebstrika
        case 529: case 530: return 2507;  // Drilbur, Excadrill
        case 546: case 547: return 2508;  // Cottonee, Whimsicott
        case 559: case 560: return 2509;  // Scraggy, Scrafty
        case 572: case 573: return 2510;  // Minccino, Cinccino
        case 577: case 578: case 579: return 2511; // Solosis, Duosion, Reuniclus
        case 595: case 596: return 2512;  // Joltik, Galvantula
        case 622: case 623: return 2513;  // Golett, Golurk
        case 677: case 678: return 2514;  // Espurr, Meowstic
        case 686: case 687: return 2515;  // Inkay, Malamar
        case 731: case 732: case 733: return 2516; // Pikipek, Trumbeak, Toucannon
        case 751: case 752: return 2517;  // Dewpider, Araquanid
        case 764: return 2518;            // Comfey
        case 774: return 2519;            // Minior
        case 868: return 2520;            // Milcery
        case 884: case 1018: return 2521; // Duraludon, Archaludon
        default: return 0;
    }
}

std::vector<RewardItem> RewardCalc::calculateRewards(uint32_t seed, uint8_t stars,
    uint64_t fixedHash, uint64_t lotteryHash,
    uint16_t species, uint8_t teraType) const
{
    std::vector<RewardItem> result;

    auto resolveItem = [&](uint8_t category, uint16_t itemId) -> uint16_t {
        if (itemId != 0) return itemId;
        if (category == 2) return getTeraShardId(teraType);    // Gem
        if (category == 1) return getMaterialId(species);       // Poke
        return 0;
    };

    // Fixed rewards (tagged with subjectType: 0=host, 1=joiner, 2=everyone)
    auto fixedIt = fixedTables_.find(fixedHash);
    if (fixedIt != fixedTables_.end()) {
        for (auto& e : fixedIt->second) {
            uint16_t id = resolveItem(e.category, e.itemId);
            if (id > 0) {
                result.push_back({id, e.amount, e.subjectType});
            }
        }
    }

    // Lottery rewards
    auto lotteryIt = lotteryTables_.find(lotteryHash);
    if (lotteryIt != lotteryTables_.end()) {
        auto& lt = lotteryIt->second;
        if (!lt.items.empty() && lt.totalRate > 0) {
            Xoroshiro128Plus rng(seed);
            int amount = getRewardCount(rng.nextInt(100), stars);

            for (int i = 0; i < amount; i++) {
                int threshold = (int)rng.nextInt((uint64_t)lt.totalRate);
                for (auto& e : lt.items) {
                    if ((int)e.rate > threshold) {
                        uint16_t id = resolveItem(e.category, e.itemId);
                        if (id > 0) {
                            result.push_back({id, e.amount, 2});
                        }
                        break;
                    }
                    threshold -= e.rate;
                }
            }
        }
    }

    return result;
}
