/*	$Id$   Greg Lee */

/* bank 1 sf */
/*  */
/*	120 Cut_Noise */
/*	121 Fl._Key_Click */
/*	122 Rain */

/* This table is derived from Eric Welsh's .cfg files for hiw "eawpats" distribution. */

#define STS 1
#define SDS 2

static int pc42bmap[][7] = {
/* Toneset?, bank, prog, newbank, newprog, note, pan */
{STS, 1, 123, 56, 76, 1, 0 }, /* Dog1oct1 */
{STS, 1, 124,  0, 124, 0, 0 }, /*  sfx/dialtone                # telephone dial */
{STS, 1, 125, 56, 63, 1, 0 }, /*  sfx/carengin                # car engine */
{STS, 1, 126, 56,  52, 1, 0 }, /*  sfx/laugh                   # laughing */
{STS, 1, 127, 56,  73, 1, 0 }, /*  sfx/machgun2                # machine gun */

{STS, 2, 120,  0, 120, 0, 0 }, /*  fx-fret                     # string slap */
/*	122 Thunder*/
{STS, 2, 123, 56,  77, 1, 0 }, /*  sfx/hoofs                   # horse gallop */
{STS, 2, 124, 56,  59, 1, 0 }, /*  sfx/creak                   # door creaking */
{STS, 2, 125, 56,  64, 1, 0 }, /*  sfx/carstop                 # car stop */
{STS, 2, 126, 56,  53, 1, 0 }, /*  sfx/scream                  # screaming */
{STS, 2, 127, 56,  81, 1, 0 }, /*  gus/sqrwave                 # laser gun */

{STS, 3, 120, 56,  49, 1, 0 }, /*  sfx/cutnoiz                 # cut noise 2 */
{STS, 3, 122, 56,  82, 1, 0 }, /*  sfx/newwind amp=75          # wind */
{STS, 3, 123, 56,  78, 1, 0 }, /*  sfx/birdtwee amp=25         # bird 2 */
{STS, 3, 124, 56,  60, 1, 0 }, /*  sfx/door                    # door slam */
{STS, 3, 125, 56,  65, 1, 0 }, /*  sfx/carpass                 # car pass */
{STS, 3, 126, 56,  54, 1, 0 }, /*  sfx/punch                   # punch */
{STS, 3, 127, 56,  75, 1, 0 }, /*  pistol                      # explosion */

{STS, 4, 120, 56,  48, 1, 0 }, /*  sfx/cutnoiz                 # dist cut noise */
{STS, 4, 122, 56,  83, 1, 0 }, /*  sfx/stream amp=75           # stream */
{STS, 4, 123, 56,  78, 1, 0 }, /*  sfx/meow           BIRD!    # kitty */
{STS, 4, 124, 56,  61, 1, 0 }, /*  sfx/scratch1                # scratch */
{STS, 4, 125, 56,  66, 1, 0 }, /*  sfx/carcrash                # car crash */
{STS, 4, 126, 56,  55, 1, 0 }, /*  sfx/heartbt                 # heart beat */
{STS, 4, 127, 56,  58, 1, 0 }, /*  sfx/firework    APPLAUSE!   # fireworks (?) */

{STS, 5, 120,  0, 120, 0, 0 }, /*  fx-fret                     # bass slide */
{STS, 5, 122, 56,  84, 1, 0 }, /*  sfx/bubbles1                # bubble */
{STS, 5, 123, 56,  76, 1, 0 }, /*  sfx/doggrowl                # growl */
{STS, 5, 124, 56,  62, 1, 0 }, /*  sfx/windchim    RIDE BELL!  # wind chime */
{STS, 5, 125, 56,  67, 1, 0 }, /*  sfx/ambulanc                # siren */
{STS, 5, 126, 56,  56, 1, 0 }, /*  mazpat/fx/newstep           # footsteps */

{STS, 6, 120,  0, 120, 0, 0 }, /*  fx-fret                     # pick strape */
{STS, 6, 124, 56,  61, 1, 0 }, /*  sfx/scratch2                # scratch 2 (?) */
{STS, 6, 125, 56,  68, 1, 0 }, /*  mazpat/fx/newtrain          # train */
{STS, 6, 126, 56,  58, 1, 0 }, /*  applause                    # applause 2 */

{STS, 7, 124, 56,  61, 1, 0 }, /*  sfx/scratch2                # scratch 2 */
{STS, 7, 125, 56,  82, 1, 0 }, /*  mazpat/fx/jet amp=70        # jetplane */

{STS, 8, 125, 56,  81, 1, 0 }, /*  sfx/starship                # starship */

{STS, 9, 125, 56,  72, 1, 0 }, /*  pistol                      # burst noise */

/* bank 120 */

{ STS, 120, 0, 56,  48, 1, 0 }, /*   sfx/cutnoiz                  # cut noise */
{ STS, 120, 1, 56,  49, 1, 0 }, /*   sfx/cutnoiz                  # cut noise 2 */
{ STS, 120, 2, 56,  48, 1, 0 }, /*   sfx/cutnoiz                  # dist cut noise */
{ STS, 120, 3,  0, 120, 0, 0 }, /*   fx-fret			# string slap */
{ STS, 120, 4,  0, 120, 0, 0 }, /*   fx-fret                      # bass slide */
{ STS, 120, 5,  0, 120, 0, 0 }, /*   fx-fret                      # pick strape */
{ STS, 120, 16, 56, 44, 1, 0 }, /*  sfx/flclick                  # fl key click */
{ STS, 120, 32, 56, 82, 1, 0 }, /*  mazpat/fx/rainyday           # rain */
{ STS, 120, 33, 56, 80, 1, 0 }, /*  sfx/thunder2			# thunder */
{ STS, 120, 34, 56, 82, 1, 0 }, /*  sfx/newwind amp=75		# wind */
{ STS, 120, 35, 56, 81, 1, 0 }, /*  sfx/stream amp=75		# stream */
{ STS, 120, 36, 56, 84, 1, 0 }, /*  sfx/bubbles1			# bubbling */
/* #37 # feed */
{ STS, 120, 48, 56, 76, 1, 0 }, /*  sfx/dog1                     # dog */
{ STS, 120, 49, 56, 77, 1, 0 }, /*  sfx/hoofs			# horse gallop */
{ STS, 120, 50, 56, 78, 1, 0 }, /*  sfx/birdtwee amp=25		# bird 2 */
{ STS, 120, 51, 56, 78, 1, 0 }, /*  sfx/meow			# kitty */
{ STS, 120, 52, 56, 76, 1, 0 }, /*  sfx/doggrowl                 # growl */
/* #53 # haunted */
{ STS, 120, 54, 56, 82, 1, 0 }, /*  sfx/ghost			# ghost */
/* #55 sfx/badmaou    		# maou */
{ STS, 120, 64,  0, 124, 0, 0 }, /*  sfx/dialtone                 # telephone dial */
{ STS, 120, 65, 56, 59, 1, 0 }, /*  sfx/creak                    # door creaking */
{ STS, 120, 66, 56, 60, 1, 0 }, /*  sfx/door                     # door slam */
{ STS, 120, 67, 56, 29, 1, 0 }, /*  sfx/scratch1                 # scratch */
{ STS, 120, 68, 56, 30, 1, 0 }, /*  sfx/scratch2                 # scratch 2 */
{ STS, 120, 69, 56, 62, 1, 0 }, /*  sfx/windchim                 # wind chime */
{ STS, 120, 70,  0, 124, 0, 0 }, /*  telephon                     # telephone 2 */
{ STS, 120, 80, 56, 63, 1, 0 }, /*  sfx/carengin                 # car engine */
{ STS, 120, 81, 56, 64, 1, 0 }, /*  sfx/carstop                  # car stop */
{ STS, 120, 82, 56, 65, 1, 0 }, /*  sfx/carpass                  # car pass */
{ STS, 120, 83, 56, 66, 1, 0 }, /*  sfx/carcrash                 # car crash */
{ STS, 120, 84, 56, 67, 1, 0 }, /*  sfx/ambulanc                 # siren */
{ STS, 120, 85, 56, 68, 1, 0 }, /*  mazpat/fx/newtrain           # train */
{ STS, 120, 86, 56, 82, 1, 0 }, /*  mazpat/fx/jet amp=70         # jetplane */
{ STS, 120, 87, 56, 81, 1, 0 }, /*  sfx/starship                 # starship */
{ STS, 120, 88, 56, 72, 1, 0 }, /*  pistol                       # burst noise */
{ STS, 120, 89, 56, 68, 1, 0 }, /*  mazpat/fx/train              # coaster */
{ STS, 120, 90, 56, 84, 1, 0 }, /*  sfx/bubbles1			# submarine (needs to be replaced) */
{ STS, 120, 96, 56, 52, 1, 0 }, /*  sfx/laugh                    # laughing */
{ STS, 120, 97, 56, 53, 1, 0 }, /*  sfx/scream                   # screaming */
{ STS, 120, 98, 56, 54, 1, 0 }, /*  sfx/punch                    # punch */
{ STS, 120, 99, 56, 55, 1, 0 }, /*  sfx/heartbt                  # heart beat */
{ STS, 120, 100, 56, 56, 1, 0 }, /*  mazpat/fx/newstep           # footsteps */
{ STS, 120, 101, 56, 58, 1, 0 }, /*  applause                    # applause 2 */
{ STS, 120, 112, 56, 73, 1, 0 }, /*  sfx/machgun2                # machine gun */
{ STS, 120, 113, 56, 69, 1, 0 }, /*  gus/sqrwave                 # laser gun */
{ STS, 120, 114, 56, 72, 1, 0 }, /*  pistol			# explosion */
{ STS, 120, 115, 56, 58, 1, 0 }, /*  sfx/firework                # fireworks */

/* drumset 0 sf	Standard */

{ SDS, 0, 13,   0, 86, 0, 0 }, /* surdo1       note=86 pan=-21 */
{ SDS, 0, 14,   0, 87, 0, 0 }, /* surdo2       note=87 pan=-32 */
{ SDS, 0, 15,   0, 27, 0, 0 }, /* highq        note=65 pan=-21 amp=50 */
{ SDS, 0, 16,   8, 28, 0, 0 }, /* slap         note=28 pan=-21 */
{ SDS, 0, 17,   0, 29, 0, 0 }, /* scratch1     note=29 pan=-19 amp=30 */
{ SDS, 0, 18,   0, 30, 0, 0 }, /* scratch2     note=30 pan=-19 amp=30 */
{ SDS, 0, 19,   0, 27, 0, 0 }, /* snap         note=65 pan=center */
{ SDS, 0, 20,   8, 32, 0, 0 }, /* sqrclick     note=60 pan=center */
{ SDS, 0, 21,   0, 33, 0, 0 }, /* metclick     note=60 pan=center */
{ SDS, 0, 22,   0, 53, 0, 0 }, /* metbell      note=60 pan=center */ /* ?? Ride Bell */
{ SDS, 0, 23,   0, 34, 0, 0 }, /* metclick     note=60 pan=center              # Seq Click L */
/* 24 metclick     note=60 pan=center              # Seq Click H */

/* # needed to remap sets 24/25 */
{ SDS, 0, 100,   0, 119, 1, 0 }, /* gsdrum25/revcym     pan=center      note=52 # reverse cymbal */
/* # repitch congas for most XG sets (verified as correct) */
{ SDS, 0, 101,   8,  63, 0, 0 }, /* congahi1            pan=-40         note=52 # +7 */
{ SDS, 0, 102,   8,  63, 0, 0 }, /* congahi2            pan=-62         note=61 # +4 */
{ SDS, 0, 103,   8,  63, 0, 0 }, /* congalo             pan=center      note=62 # low */
/* # conga pitches for XG Analog set (verified as correct) */
{ SDS, 0, 104,   8,  63, 0, 0 }, /* congahi1            pan=-40         note=55 # +12 */
{ SDS, 0, 105,   8,  63, 0, 0 }, /* congahi2            pan=-62         note=62 # +7 */
{ SDS, 0, 106,   8,  63, 0, 0 }, /* congalo             pan=center      note=60 # low */

/* # Dance */
/* drumset 26 */
/*  */
/* # like the Electric set but with CR-78 drums */

{ SDS, 26, 35,  24, 35, 0, 0 }, /* gsdrum25/78kick      pan=center      amp=100 note=36 */
{ SDS, 26, 36,  24, 36, 0, 0 }, /* gsdrum25/808bd2      pan=center      amp=100 */
{ SDS, 26, 38,  24, 38, 0, 0 }, /* gsdrum25/78snare     pan=center */
{ SDS, 26, 40,  24, 40, 0, 0 }, /* gsdrum25/909snare    pan=center              note=38 */
{ SDS, 26, 41,  24, 41, 0, 0 }, /* gsdrum25/808toml2    pan=-63 */
{ SDS, 26, 42,  24, 42, 0, 0 }, /* gsdrum25/78hhc       pan=21 */
{ SDS, 26, 43,  24, 43, 0, 0 }, /* gsdrum25/808toml1    pan=-40 */
{ SDS, 26, 45,  24, 45, 0, 0 }, /* gsdrum25/808tomm2    pan=-19 */
{ SDS, 26, 46,  24, 46, 0, 0 }, /* gsdrum25/78hho       pan=21 */
{ SDS, 26, 47,  24, 47, 0, 0 }, /* gsdrum25/808tomm1    pan=center */
{ SDS, 26, 48,  24, 48, 0, 0 }, /* gsdrum25/808tomh2    pan=30 */
{ SDS, 26, 50,  24, 50, 0, 0 }, /* gsdrum25/808tomh1    pan=63 */
{ SDS, 26, 52,  24, 52, 0, 0 }, /* gsdrum25/revcym      pan=center */

/*  */
/* # Roland SC-88 Ethnic Set */
/* drumset 49 */
/*  */
/* # Normal drums should be ok, but most of the exotic drums sound wrong.  I */
/* # have a set of reference samples from an SC-88 now, so I will improve the */
/* # exotic drum assignments in the future.  There are two mappings for the */
/* # tablas.  The bongo/conga mapping is taken from the included tabla.txt.  */
/* # The tabla.pat tabla mapping is based off the bongo/conga tabla mapping, */
/* # where I tried to assign the pitches by ear to those heard in the bongo/conga */
/* # mapping.  I hadn't done this correctly in some previous releases, but */
/* # hopefully I've got it right now.  The pitches of the two different mappings */
/* # should sound the same. */

{ SDS, 49, 25,   8, 26, 0, 0 }, /* snap note=65		pan=center	# Finger Snap */
{ SDS, 49, 26,   0, 54, 0, 0 }, /* tamborin	note=60	pan=center	# Tambourine */
{ SDS, 49, 27,   0, 85, 0, 0 }, /* castinet note=85	pan=center	# Castanets */
{ SDS, 49, 28,   0, 55, 0, 0 }, /* cymcrsh1 note=49	pan=8	amp=90	# Crash Cymbal1 */
{ SDS, 49, 29,   8, 41, 0, 0 }, /* snarerol keep=env note=60 amp=225 pan=center	# Snare Roll  ?? */
{ SDS, 49, 30,  99, 38, 0, 0 }, /* snare1 note=38	pan=center	# Concert Snare Drum */
{ SDS, 49, 31,   0, 55, 0, 0 }, /* cymcrsh2 note=57	pan=-21	amp=90	# Concert Cymbal */
{ SDS, 49, 32,   0, 35, 0, 0 }, /* concrtbd note=48	pan=center	# Concert BD1 */
{ SDS, 49, 33,   0, 83, 0, 0 }, /* jingles	note=92	pan=65		# Jingle Bell */
{ SDS, 49, 34,   8, 84, 0, 0 }, /* belltree note=84	pan=center keep=loop keep=env	# Bell Tree */
{ SDS, 49, 35,   8, 84, 0, 0 }, /* belltree note=84	keep=loop keep=env		# Bar Chimes */
{ SDS, 49, 36,  99, 47, 0, 0 }, /* tommid1 note=47		# Wadaiko */
{ SDS, 49, 37,   0, 37, 0, 0 }, /* stickrim note=37		# Wadaiko Rim */
{ SDS, 49, 38,  99, 47, 0, 0 }, /* tommid1 note=47		# Shimo Taiko */
{ SDS, 49, 39,  99, 47, 0, 0 }, /* tommid1 note=47		# Atarigana */
{ SDS, 49, 40,  99, 48, 0, 0 }, /* tommid1 note=47		# Hyoushigi */
{ SDS, 49, 41,  99, 48, 0, 0 }, /* tommid1 note=47		# Ohkawa */
{ SDS, 49, 42,   8, 63, 0, 0 }, /* congahi2 note=62		# High Kotsuzumi */
{ SDS, 49, 43,   8, 63, 0, 0 }, /* congalo note=60		# Low Kotsuzumi */
{ SDS, 49, 44,  99, 50, 0, 0 }, /* tommid1 note=47		# Ban Gu */
{ SDS, 49, 45,   0, 14, 1, 0 }, /* ethnic/gong note=59		# Big Gong */
{ SDS, 49, 46,   0, 14, 1, 0 }, /* ethnic/gong note=59		# Small Gong */
{ SDS, 49, 47,   0, 14, 1, 0 }, /* ethnic/gong note=59 		# Bend Gong */
{ SDS, 49, 48,   0, 14, 1, 0 }, /* ethnic/gong note=59 		# Thai Gong */
{ SDS, 49, 49,   8, 52, 1, 0 }, /* cymchina note=52 		# Rama Cymbal */
{ SDS, 49, 50,   8, 52, 0, 0 }, /* cymchina note=52 		# Gamelon Gong */
{ SDS, 49, 51,  99, 47, 0, 0 }, /* tommid1 note=47		# Udo Short */
{ SDS, 49, 52,  99, 48, 0, 0 }, /* tommid1 note=47		# Udo Long */
{ SDS, 49, 53,   8, 28, 0, 0 }, /* slap note=28			# Udo Slap */
{ SDS, 49, 54,   8, 63, 0, 0 }, /* congalo note=60		# Bendir */
{ SDS, 49, 55,   8, 63, 0, 0 }, /* congalo note=60		# Rec Dum */
{ SDS, 49, 56,   8, 63, 0, 0 }, /* congalo note=60		# Rec Tik */
{ SDS, 49, 57,   0, 104, 1, 0 }, /* ethnic/tabla note=60		# Tabla Te */
{ SDS, 49, 58,   0, 104, 1, 0 }, /* ethnic/tabla note=52		# Tabla Na */
{ SDS, 49, 59,   0, 104, 1, 0 }, /* ethnic/tabla note=50		# Tabla Tun */
{ SDS, 49, 60,   0, 104, 1, 0 }, /* ethnic/tabla note=43		# Tabla Ge */
{ SDS, 49, 61,   0, 104, 1, 0 }, /* ethnic/tabla note=55		# Tabla Ge Hi */
{ SDS, 49, 62,   0, 117, 1, 0 }, /* ethnic/talkdrum note=64	# Talking Drum */
{ SDS, 49, 63,   0, 117, 1, 0 }, /* ethnic/talkbend note=51	# Bend Talking Drum */
{ SDS, 49, 64,   8, 63, 0, 0 }, /* congalo note=60		# Caxixi */
{ SDS, 49, 65,  99, 61, 0, 0 }, /* bongohi note=60		# Djembe */
{ SDS, 49, 66,  99, 60, 0, 0 }, /* stickrim note=37		# Djembe Rim */
{ SDS, 49, 67,  99, 66, 0, 0 }, /* timbalel note=60	pan=center	# Timbales Low */
{ SDS, 49, 68,   0, 66, 0, 0 }, /* timbalel note=60	pan=center	# Timbales Paila */
{ SDS, 49, 69,   0, 65, 0, 0 }, /* timbaleh note=60	pan=center	# Timbales High */
{ SDS, 49, 70,   8, 56, 0, 0 }, /* cowbell note=56	pan=21		# Cowbell */
{ SDS, 49, 71,  99, 61, 0, 0 }, /* bongohi note=60	pan=73		# Hi Bongo */
{ SDS, 49, 72,  99, 61, 0, 0 }, /* bongolo note=61	pan=73		# Low Bongo */
{ SDS, 49, 73,   8, 63, 0, 0 }, /* congahi1 note=55	pan=-40		# Mute Hi Conga */
{ SDS, 49, 74,   8, 63, 0, 0 }, /* congahi2 note=62	pan=-62		# Open Hi Conga */
{ SDS, 49, 75,   8, 63, 0, 0 }, /* congalo note=60	pan=center	# Mute Low Conga */
{ SDS, 49, 76,   8, 63, 0, 0 }, /* congalo note=60	pan=center	# Conga Slap */
{ SDS, 49, 77,   8, 63, 0, 0 }, /* congalo note=60	pan=center	# Open Low Conga */
{ SDS, 49, 78,   8, 63, 0, 0 }, /* congalo note=60	pan=center	# Conga Slide */
{ SDS, 49, 79,   0, 86, 0, 0 }, /* surdo1 note=86	pan=-21		# Mute Pandiero */
{ SDS, 49, 80,   0, 87, 0, 0 }, /* surdo2 note=87	pan=-21		# Open Pandiero */
{ SDS, 49, 81,   0, 87, 0, 0 }, /* surdo2 note=87	pan=-21		# Open Surdo */
{ SDS, 49, 82,   0, 86, 0, 0 }, /* surdo1 note=86	pan=-21		# Mute Surdo */
{ SDS, 49, 83,   0, 54, 0, 0 }, /* tamborin note=60	pan=center	# Tamborim */
{ SDS, 49, 84,  99, 67, 0, 0 }, /* agogohi note=60	pan=-48		# High Agogo */
{ SDS, 49, 85,  99, 68, 0, 0 }, /* agogolo note=60	pan=-48		# Low Agogo */
{ SDS, 49, 86,  99, 82, 0, 0 }, /* shaker note=82	pan=30		# Shaker */
{ SDS, 49, 87,  99, 71, 0, 0 }, /* whistle1 note=60	pan=59	keep=loop keep=env	# High Whistle */
{ SDS, 49, 88,  99, 72, 0, 0 }, /* whistle2 note=60	pan=59	keep=loop keep=env	# Low Whistle */
{ SDS, 49, 89,  99, 78, 0, 0 }, /* cuica1 note=60	pan=-68		# Mute Cuica */
{ SDS, 49, 90,  99, 79, 0, 0 }, /* cuica2 note=79	pan=-48		# Open Cuica */
{ SDS, 49, 91,  99, 80, 0, 0 }, /* triangl1 note=60	pan=-62		# Mute Triangle */
{ SDS, 49, 92,  99, 81, 0, 0 }, /* triangl2 note=60	pan=-62		# Open Triangle */
{ SDS, 49, 93,  99, 73, 0, 0 }, /* guiro1 note=73	pan=49		# Short Guiro */
{ SDS, 49, 94,  99, 74, 0, 0 }, /* guiro2 note=60	pan=73	keep=env	# Long Guiro */
{ SDS, 49, 95,  99, 69, 0, 0 }, /* cabasa note=69	pan=-57		# Cabasa Up */
{ SDS, 49, 96,  99, 69, 0, 0 }, /* cabasa note=69	pan=-57		# Cabasa Down */
{ SDS, 49, 97,  99, 75, 0, 0 }, /* clave note=75	pan=center	# Claves */
{ SDS, 49, 98,  99, 76, 0, 0 }, /* woodblk1 note=60	pan=63		# High Wood Block */
{ SDS, 49, 99,  99, 77, 0, 0 }, /* woodblk2 note=60	pan=63		# Low Wood Block */

/* # Roland SC-88 Kick & Snare Set */
/* drumset 50 */

{ SDS, 50, 40,  99, 36, 0, 0 }, /* kick1 note=35	pan=center	# Standard 1 Kick 1 */
{ SDS, 50, 41,  99, 35, 0, 0 }, /* kick2 note=36	pan=center	# Standard 1 Kick 2 */
{ SDS, 50, 42,  99, 36, 0, 0 }, /* kick1 note=35	pan=center	# Standard 2 Kick 1 */
{ SDS, 50, 43,  99, 35, 0, 0 }, /* kick2 note=36	pan=center	# Standard 2 Kick 2 */
{ SDS, 50, 44,  99, 36, 0, 0 }, /* kick1 note=35	pan=center	# Kick 1 */
{ SDS, 50, 45,  99, 35, 0, 0 }, /* kick2 note=36	pan=center	# Kick 2 */
{ SDS, 50, 46,  99, 35, 0, 0 }, /* kick2 note=36	pan=center	# Soft Kick */
{ SDS, 50, 47,  99, 36, 0, 0 }, /* kick1 note=35	pan=center	# Jazz Kick 1 */
{ SDS, 50, 48,  99, 35, 0, 0 }, /* kick2 note=36	pan=center	# Jazz Kick 2 */
{ SDS, 50, 49,   0, 35, 0, 0 }, /* concrtbd note=48	pan=center	# Concert BD */
{ SDS, 50, 50,   8, 36, 0, 0 }, /* kick1 note=35	pan=center	# Room Kick 1 */
{ SDS, 50, 51,   8, 35, 0, 0 }, /* gsdrum08/roomkick note=36 pan=center	# Room Kick 2 */
{ SDS, 50, 52,  16, 36, 0, 0 }, /* power/powrkic1     amp=150 note=36 pan=center # Power Kick 1 */
{ SDS, 50, 53,  16, 35, 0, 0 }, /* power/powrkic3     amp=150 note=36 pan=center # Power Kick 2 */
{ SDS, 50, 54,  24, 35, 0, 0 }, /* power/powrkic1     amp=150 note=36 pan=center # Electric Kick 2 (1?) */
{ SDS, 50, 55,  24, 36, 0, 0 }, /* power/powrkic3     amp=150 note=36 pan=center # Electric Kick 1 (2?) */
{ SDS, 50, 56,  24, 36, 0, 0 }, /* power/powrkic1     amp=150 note=36 pan=center # Electric Kick */
{ SDS, 50, 57,  25, 36, 0, 0 }, /* gsdrum25/808bd2    amp=100 note=36 pan=center # 808 Bass Drum */
{ SDS, 50, 58,  25, 35, 0, 0 }, /* gsdrum25/909kick   amp=100 note=36 pan=center # 909 Bass Drum */
{ SDS, 50, 59,  32, 35, 0, 0 }, /* gsdrum25/78kick    amp=100 note=36 pan=center # Dance Kick */
{ SDS, 50, 60,  32, 38, 0, 0 }, /* snare1 note=38	pan=center	# Standard 1 Snare 1 */
{ SDS, 50, 61,  32, 40, 0, 0 }, /* snare2 note=40	pan=center	# Standard 1 Snare 2 */
{ SDS, 50, 62,  32, 38, 0, 0 }, /* snare1 note=38	pan=center	# Standard 2 Snare 1 */
{ SDS, 50, 63,  32, 40, 0, 0 }, /* snare2 note=40	pan=center	# Standard 2 Snare 2 */
{ SDS, 50, 64,  32, 40, 0, 0 }, /* snare2 note=40	pan=center	# Tight Snare */
{ SDS, 50, 65,  32, 38, 0, 0 }, /* snare1 note=38	pan=center	# Concert Snare */
{ SDS, 50, 66,  32, 38, 0, 0 }, /* snare1 note=38	pan=center	# Jazz Snare 1 */
{ SDS, 50, 67,  32, 40, 0, 0 }, /* snare2 note=40	pan=center	# Jazz Snare 2 */
{ SDS, 50, 68,  32, 38, 0, 0 }, /* snare1 note=38	pan=center	# Room Snare 1 */
{ SDS, 50, 69,  32, 40, 0, 0 }, /* snare2 note=40	pan=center	# Room Snare 2 */
{ SDS, 50, 70,  16, 38, 0, 0 }, /* power/gatesd1	pan=center note=38 # Power Snare 1 */
{ SDS, 50, 71,  16, 40, 0, 0 }, /* power/gatesd0	pan=center note=38 # Power Snare 2 */
{ SDS, 50, 72,  16, 39, 0, 0 }, /* power/gatesd2	pan=center note=38 # Gated Snare */
{ SDS, 50, 73,  32, 38, 0, 0 }, /* gsdrum25/78snare	pan=center note=38 # Dance Snare 1 */
{ SDS, 50, 74,  32, 40, 0, 0 }, /* gsdrum25/909snare	pan=center note=40 # Dance Snare 2 */
{ SDS, 50, 75,  32, 40, 0, 0 }, /* snare1		pan=center note=38 # Disco Snare */
{ SDS, 50, 76,  24, 40, 0, 0 }, /* power/gatesd0	pan=center note=38 # Electric Snare 2 */
{ SDS, 50, 77,  24, 39, 0, 0 }, /* power/gatesd1	pan=center note=38 # House Snare */
{ SDS, 50, 78,  24, 38, 0, 0 }, /* power/gatesd1	pan=center note=38 # Electric Snare 1 */
{ SDS, 50, 79,  24, 39, 0, 0 }, /* power/gatesd0	pan=center note=38 # Electric Snare 3 */
{ SDS, 50, 80,  25, 38, 0, 0 }, /* gsdrum25/808snare	pan=center note=38 # 808 Snare 1 */
{ SDS, 50, 81,  25, 40, 0, 0 }, /* gsdrum25/808snar2	pan=center note=40 # 808 Snare 2 */
{ SDS, 50, 82,  25, 38, 0, 0 }, /* gsdrum25/909snare	pan=center note=38 # 909 Snare 1 */
{ SDS, 50, 83,  25, 40, 0, 0 }, /* gsdrum25/808snar2	pan=center note=40 # 909 Snare 2 */
{ SDS, 50, 84,  40, 33, 0, 0 }, /* gsdrum40/br_swish	amp=70	pan=center note=38 # Brush Tap1 */
{ SDS, 50, 85,  40, 34, 0, 0 }, /* gsdrum40/br_swish	amp=70	pan=center note=38 # Brush Tap2 */
{ SDS, 50, 86,  40, 39, 0, 0 }, /* gsdrum40/br_slap	pan=center note=39 # Brush Slap1 */
{ SDS, 50, 87,  40, 39, 0, 0 }, /* gsdrum40/br_slap	pan=center note=39 # Brush Slap2 */
{ SDS, 50, 88,  40, 39, 0, 0 }, /* gsdrum40/br_slap	pan=center note=39 # Brush Slap3 */
{ SDS, 50, 89,  40, 40, 0, 0 }, /* gsdrum40/br_swirl	amp=70	pan=center note=40 keep=env # Brush Swirl1 */
{ SDS, 50, 90,  40, 40, 0, 0 }, /* gsdrum40/br_swirl	amp=70	pan=center note=40 keep=env # Brush Swirl2 */
{ SDS, 50, 91,  40, 40, 0, 0 }, /* gsdrum40/br_swirl	amp=70	pan=center note=40 keep=env # Brush Long Swirl */

/* drumset 56 sf	SFX_[SC-55] */

{ SDS, 56, 31,  56, 42, 0, 0 }, /* sfx/scratch1         note=60         pan=center      # Scratch Push2 */
{ SDS, 56, 32,  56, 41, 0, 0 }, /* sfx/scratch2         note=60         pan=center      # Scratch Pull2 */
{ SDS, 56, 33,  56, 49, 0, 0 }, /* sfx/cutnoiz          note=60         pan=center      # Cutting Noise 2 Up */
{ SDS, 56, 34,  56, 48, 0, 0 }, /* sfx/cutnoiz          note=60         pan=center      # Cutting Noise 2 Down */
{ SDS, 56, 35,  56, 49, 0, 0 }, /* sfx/cutnoiz          note=60         pan=center      # DistGuitar Cutting Noise Up */
{ SDS, 56, 36,  56, 48, 0, 0 }, /* sfx/cutnoiz          note=60         pan=center      # DistGuitar Cutting Noise Down */
{ SDS, 56, 37,   0, 120, 1, 0 }, /* fx-fret              note=60         pan=center      # Bass Slide */
{ SDS, 56, 38,   0, 120, 1, 0 }, /* fx-fret              note=60         pan=center      # Pick Scrape */


{ SDS, 56, 85, 56, 78, 0, 0 }, /*  sfx/meow             note=60         pan=center      # Kitty */
{ SDS, 56, 86, 56, 78, 0, 0 }, /*  sfx/birdtwee amp=25  note=60 keep=loop keep=env pan=center   # Bird2 */
{ SDS, 56, 87, 56, 76, 0, 0 }, /*  sfx/doggrowl         note=60         pan=center      # Growl */
{ SDS, 56, 88, 56, 58, 0, 0 }, /*  applause             note=60 keep=loop keep=env pan=center   # Applause2 */
{ SDS, 56, 89, 0, 124, 1, 0 }, /*  telephon             note=60         pan=center      # Telephone1 */
{ SDS, 56, 90, 0, 124, 1, 0 }, /*  sfx/dialtone         note=60 keep=loop keep=env pan=center   # Telephone2 (dial) */


/* drumset 121 */

{ SDS, 121, 36, 56, 49, 0, 0 }, /*  sfx/cutnoiz					note=60	# cut noise */
{ SDS, 121, 37, 56, 48, 0, 0 }, /*  sfx/cutnoiz					note=60	# cut noise 2 */
{ SDS, 121, 38, 56, 48, 0, 0 }, /*  sfx/cutnoiz					note=60	# dist cut noise */
{ SDS, 121, 39,  0, 120, 1, 0 }, /*  fx-fret					note=60	# string slap */
{ SDS, 121, 40,  0, 120, 1, 0 }, /*  fx-fret					note=60	# bass slide */
{ SDS, 121, 41,  0, 120, 1, 0 }, /*  fx-fret					note=60	# pick slide */
{ SDS, 121, 52, 56, 44, 0, 0 }, /*  sfx/flclick					note=60	# fl key click */
{ SDS, 121, 68, 56, 81, 0, 0 }, /*  mazpat/fx/rainyday	keep=loop keep=env	note=60	# rain */
{ SDS, 121, 69, 56, 80, 0, 0 }, /*  sfx/thunder2		keep=loop keep=env	note=60	# thunder */
{ SDS, 121, 70, 56, 82, 0, 0 }, /*  sfx/newwind	amp=75	keep=loop keep=env	note=60	# wind */
{ SDS, 121, 71, 56, 83, 0, 0 }, /*  sfx/stream 	amp=75	keep=loop keep=env	note=60	# stream */
{ SDS, 121, 72, 56, 84, 0, 0 }, /*  sfx/bubbles1 	keep=loop keep=env	note=48 # bubble */
/* #73 # feed */
{ SDS, 121, 84, 56, 76, 0, 0 }, /*  sfx/dog1					note=60	# dog */
{ SDS, 121, 85, 56, 77, 0, 0 }, /*  sfx/hoofs		keep=loop keep=env	note=60	# horse gallop */
{ SDS, 121, 86, 56, 78, 0, 0 }, /*  sfx/birdtwee amp=25	keep=loop keep=env	note=60	# bird 2 */
{ SDS, 121, 87, 56, 78, 0, 0 }, /*  sfx/meow					note=60 # kitty */
{ SDS, 121, 88, 56, 76, 0, 0 }, /*  sfx/doggrowl					note=60 # growl */
/* #89 # haunted */
{ SDS, 121, 90, 56, 82, 0, 0 }, /*  sfx/ghost					note=60 # ghost */
/* #91 sfx/badmaou		keep=loop keep=env	note=60 # maou */

/* drumset 122 */

{ SDS, 122, 36, 0, 124, 1, 0 }, /*  telephon					note=60	# telephone 2 */
{ SDS, 122, 37, 56, 59, 0, 0 }, /*  sfx/creak					note=60	# door creaking */
{ SDS, 122, 38, 56, 60, 0, 0 }, /*  sfx/door					note=60	# door slam */
{ SDS, 122, 39, 56, 61, 0, 0 }, /*  sfx/scratch1					note=60	# scratch */
{ SDS, 122, 40, 56, 61, 0, 0 }, /*  sfx/scratch2					note=60	# scratch 2 */
{ SDS, 122, 41, 56, 62, 0, 0 }, /*  sfx/windchim		keep=loop keep=env	note=60	# wind chime */
{ SDS, 122, 42, 0, 124, 1, 0 }, /*  telephon					note=60	# telephone ring */
{ SDS, 122, 52, 56, 63, 0, 0 }, /*  sfx/carengin					note=60	# engine start */
{ SDS, 122, 53, 56, 64, 0, 0 }, /*  sfx/carstop					note=60	# stop */
{ SDS, 122, 54, 56, 65, 0, 0 }, /*  sfx/carpass					note=60	# car pass */
{ SDS, 122, 55, 56, 66, 0, 0 }, /*  sfx/carcrash					note=60	# crash */
{ SDS, 122, 56, 56, 67, 0, 0 }, /*  sfx/ambulanc		keep=loop keep=env	note=60	# siren */
{ SDS, 122, 57, 56, 68, 0, 0 }, /*  mazpat/fx/newtrain	keep=loop keep=env	note=60	# train */
{ SDS, 122, 58, 56, 82, 0, 0 }, /*  mazpat/fx/jet amp=70	keep=loop keep=env	note=60	# jetplane */
{ SDS, 122, 59, 56, 82, 0, 0 }, /*  sfx/starship		keep=loop keep=env	note=60	# starship */
{ SDS, 122, 60, 56, 72, 0, 0 }, /*  pistol					note=60	# burst noise */
{ SDS, 122, 61, 56, 68, 0, 0 }, /*  mazpat/fx/train      keep=loop keep=env	note=60 # coaster */
{ SDS, 122, 62, 56, 84, 0, 0 }, /*  sfx/bubbles1		keep=loop keep=env	note=48	# submarine (needs to be replaced */
{ SDS, 122, 68, 56, 52, 0, 0 }, /*  sfx/laugh					note=60	# laughing */
{ SDS, 122, 69, 56, 53, 0, 0 }, /*  sfx/scream					note=60	# screaming */
{ SDS, 122, 70, 56, 54, 0, 0 }, /*  sfx/punch					note=60	# punch */
{ SDS, 122, 71, 56, 55, 0, 0 }, /*  sfx/heartbt					note=60	# heart beat */
{ SDS, 122, 72, 56, 56, 0, 0 }, /*  mazpat/fx/newstep				note=60	# footsteps */
{ SDS, 122, 73, 56, 58, 0, 0 }, /*  applause		keep=loop keep=env	note=60	# applause */
{ SDS, 122, 84, 56, 73, 0, 0 }, /*  sfx/machgun2		keep=loop keep=env	note=60	# machine gun */
{ SDS, 122, 85, 56, 69, 0, 0 }, /*  gus/sqrwave					note=60	# laser gun */
{ SDS, 122, 86, 56, 72, 0, 0 }, /*  pistol					note=60	# explosion */
{ SDS, 122, 87, 56, 81, 0, 0 }, /*  sfx/firework					note=60	# fireworks */

/* drumset 127             # GS XM-64/32L drumset, half drums, half SFX */

{ SDS, 127, 76, 56, 52, 0, 0 }, /*  sfx/laugh            note=60         pan=center      # Laughing */
{ SDS, 127, 77, 56, 53, 0, 0 }, /*  sfx/scream           note=60         pan=center      # Scream */
{ SDS, 127, 78, 56, 54, 0, 0 }, /*  sfx/punch            note=60         pan=center      # Punch */
{ SDS, 127, 79, 56, 55, 0, 0 }, /*  sfx/heartbt          note=60         pan=center      # Heart Beat */
{ SDS, 127, 80, 56, 56, 0, 0 }, /*  mazpat/fx/newstep    note=60         pan=center      # Footsteps1 */
{ SDS, 127, 81, 56, 56, 0, 0 }, /*  mazpat/fx/newstep    note=60         pan=center      # Footsteps2 */
{ SDS, 127, 82, 56, 58, 0, 0 }, /*  applause             note=60 keep=loop keep=env pan=center   # Applause */
{ SDS, 127, 83, 56, 83, 0, 0 }, /*  sfx/creak            note=60         pan=center      # Door Creaking */
{ SDS, 127, 84, 56, 84, 0, 0 }, /*  sfx/door             note=60         pan=center      # Door */
{ SDS, 127, 85, 56, 85, 0, 0 }, /*  sfx/scratch2         note=60         pan=center      # Scratch */
{ SDS, 127, 86, 56, 62, 0, 0 }, /*  sfx/windchim         note=60 keep=loop keep=env pan=center   # Wind Chimes */
{ SDS, 127, 87, 56, 63, 0, 0 }, /*  sfx/carengin         note=60         pan=center      # Car-Engine */
{ SDS, 127, 88, 56, 64, 0, 0 }, /*  sfx/carstop          note=60         pan=center      # Car-Stop */
{ SDS, 127, 89, 56, 65, 0, 0 }, /*  sfx/carpass          note=60         pan=center      # Car-Pass */
{ SDS, 127, 90, 56, 66, 0, 0 }, /*  sfx/carcrash         note=60         pan=center      # Car-Crash */
{ SDS, 127, 91, 56, 67, 0, 0 }, /*  sfx/ambulanc         note=60 keep=loop keep=env pan=center   # Siren */
{ SDS, 127, 92, 56, 68, 0, 0 }, /*  mazpat/fx/newtrain   note=60 keep=loop keep=env pan=center   # Train */
{ SDS, 127, 93, 56, 82, 0, 0 }, /*  mazpat/fx/jet amp=70 note=60 keep=loop keep=env pan=center   # Jetplane */
{ SDS, 127, 94, 56, 70, 0, 0 }, /*  helicptr             note=60 keep=loop keep=env pan=center   # Helicopter */
{ SDS, 127, 95, 56, 81, 0, 0 }, /*  sfx/starship         note=60 keep=loop keep=env pan=center   # Starship */
{ SDS, 127, 96, 56, 72, 0, 0 }, /*  pistol               note=60         pan=center      # Gun Shot */
{ SDS, 127, 97, 56, 73, 0, 0 }, /*  sfx/machgun2         note=60 keep=loop keep=env pan=center   # Machine Gun */
{ SDS, 127, 98, 56, 69, 0, 0 }, /*  gus/sqrwave          note=60         pan=center      # Lasergun */
{ SDS, 127, 99, 56, 72, 0, 0 }, /*  pistol               note=60         pan=center      # Explosion */
{ SDS, 127, 100, 56, 76, 0, 0 }, /*  sfx/dog1            note=60         pan=center      # Dog */
{ SDS, 127, 101, 56, 77, 0, 0 }, /*  sfx/hoofs           note=60 keep=loop keep=env pan=center   # Horse-Gallop */
{ SDS, 127, 102, 56, 78, 0, 0 }, /*  sfx/birdtwee amp=25 note=60 keep=loop keep=env pan=center   # Birds */
{ SDS, 127, 103, 56, 81, 0, 0 }, /*  mazpat/fx/rainyday  note=60 keep=loop keep=env pan=center   # Rain */
{ SDS, 127, 104, 56, 80, 0, 0 }, /*  sfx/thunder2        note=60 keep=loop keep=env pan=center   # Thunder */
{ SDS, 127, 105, 56, 81, 0, 0 }, /*  sfx/newwind amp=75  note=60 keep=loop keep=env pan=center   # Wind */
{ SDS, 127, 106, 56, 82, 0, 0 }, /*  seashore            note=60 keep=loop keep=env pan=center   # Seashore */
{ SDS, 127, 107, 56, 83, 0, 0 }, /*  sfx/stream amp=75   note=60 keep=loop keep=env pan=center   # Stream */
{ SDS, 127, 108, 56, 84, 0, 0 }, /*  sfx/bubbles1        note=48 keep=loop keep=env pan=center   # Bubble */
{   0,   0,   0,  0,   0, 0, 0 }
};

#include <stdio.h>

void pcmap(int *b, int *v, int *p, int *drums) {
	int bank = *b;
	int voi = *v;
	int i, bktype;
	if (!*drums) voi = *p;
	for (i = 0; ; i++) {
		bktype = pc42bmap[i][0];
		if (!bktype) return;
		if (*drums && (bktype != SDS)) continue;
		if (!*drums && (bktype != STS)) continue;
		if (bank != pc42bmap[i][1]) continue;
		if (voi != pc42bmap[i][2]) continue;
		*b = pc42bmap[i][3];
		if (*drums) *v = pc42bmap[i][4];
		else *p = pc42bmap[i][4];
		if (bktype == SDS && pc42bmap[i][5]) *drums = 0;
		else if (bktype == STS && pc42bmap[i][5]) *drums = 1;
		return;
	}
}
