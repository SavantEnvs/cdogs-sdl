/* Shared game-global initialisation for the campaign loaders (fuzz_campaign, fuzz_archive): the
 * built-in data classes the game loads at start-up (src/cdogs.c main), headless. After this they
 * are read-only except the "custom" classes a campaign archive adds, which
 * CampaignSettingTerminateAll clears again (the game does the same between campaigns). */
#ifndef CDOGS_FUZZ_GAME_INIT_H
#define CDOGS_FUZZ_GAME_INIT_H
#include <SDL.h>
#include <ammo.h>
#include <bullet_class.h>
#include <campaigns.h>
#include <character_class.h>
#include <config.h>
#include <grafx.h>
#include <map_new.h>
#include <map_object.h>
#include <particle_class.h>
#include <pic_manager.h>
#include <pickup_class.h>
#include <player_template.h>
#include <sounds.h>
#include <weapon_class.h>
#include <draw/char_sprites.h>

static void fuzz_game_init(void)
{
	/* Headless graphics device: SDL's dummy video driver with its software renderer. Pictures
	 * need the device's pixel format and a renderer to make textures (pic.c), as in the game. */
	setenv("SDL_VIDEODRIVER", "dummy", 1);
	setenv("SDL_AUDIODRIVER", "dummy", 1);
	setenv("SDL_RENDER_DRIVER", "software", 1);
	gConfig = ConfigDefault();
	if (SDL_Init(SDL_INIT_VIDEO) != 0)
		abort();
	GraphicsInit(&gGraphicsDevice, &gConfig);
	GraphicsInitialize(&gGraphicsDevice);
	if (!gGraphicsDevice.IsInitialized)
		abort();
	PicManagerInit(&gPicManager);
	PicManagerLoad(&gPicManager);
	CharSpriteClassesInit(&gCharSpriteClasses);
	ParticleClassesInit(&gParticleClasses, "data/particles.json");
	AmmoInitialize(&gAmmo, "data/ammo.json");
	BulletAndWeaponInitialize(&gBulletClasses, &gWeaponClasses, "data/bullets.json",
	                          "data/guns.json");
	CharacterClassesInitialize(&gCharacterClasses, "data/character_classes.json");
	PickupClassesInit(&gPickupClasses, "data/pickups.json", &gAmmo, &gWeaponClasses);
	MapObjectsInit(&gMapObjects, "data/map_objects.json", &gAmmo, &gWeaponClasses);
	/* Empty sound tables (no audio device is opened; sounds are looked up by name). */
	gSoundDevice.sounds = hashmap_new();
	gSoundDevice.customSounds = hashmap_new();
	/* Player templates come from the user config dir in the game; start with none. */
	CArrayInit(&gPlayerTemplates.Classes, sizeof(PlayerTemplate));
	CArrayInit(&gPlayerTemplates.CustomClasses, sizeof(PlayerTemplate));
}

#endif
