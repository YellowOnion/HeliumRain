
#include "FlareSoundManager.h"
#include "../Flare.h"

#include "FlareMenuManager.h"
#include "FlarePlayerController.h"

#include "../Spacecrafts/FlareSpacecraft.h"
#include "../Spacecrafts/FlareOrbitalEngine.h"

#include "AudioDevice.h"


#define LOCTEXT_NAMESPACE "UFlareSoundManager"


/*----------------------------------------------------
	Constructor
----------------------------------------------------*/

UFlareSoundManager::UFlareSoundManager(const class FObjectInitializer& PCIP)
	: Super(PCIP)
{
	// Gameplay data
	ShipPawn = NULL;
	MasterVolume = 1.0f;
	MusicVolume = 1.0f;
	EffectsVolume = 1.0f;
	MusicChanging = false;
	MusicDesiredTrack = EFlareMusicTrack::None;

	// Music track references
	static ConstructorHelpers::FObjectFinder<USoundCue> MenuMusicObj(TEXT("/Game/Music/A_Menu_Cue"));
	static ConstructorHelpers::FObjectFinder<USoundCue> IntroMusicObj(TEXT("/Game/Music/A_Intro_Cue"));
	static ConstructorHelpers::FObjectFinder<USoundCue> Ambient1MusicObj(TEXT("/Game/Music/A_Ambient1_Cue"));
	static ConstructorHelpers::FObjectFinder<USoundCue> Ambient2MusicObj(TEXT("/Game/Music/A_Ambient2_Cue"));
	static ConstructorHelpers::FObjectFinder<USoundCue> Travel1MusicObj(TEXT("/Game/Music/A_Travel1_Cue"));
	static ConstructorHelpers::FObjectFinder<USoundCue> Travel2MusicObj(TEXT("/Game/Music/A_Travel2_Cue"));
	static ConstructorHelpers::FObjectFinder<USoundCue> ExplorationMusicObj(TEXT("/Game/Music/A_Exploration_Cue"));
	static ConstructorHelpers::FObjectFinder<USoundCue> DangerMusicObj(TEXT("/Game/Music/A_Danger_Cue"));
	static ConstructorHelpers::FObjectFinder<USoundCue> CombatMusicObj(TEXT("/Game/Music/A_Combat_Cue"));
	static ConstructorHelpers::FObjectFinder<USoundCue> BattleMusicObj(TEXT("/Game/Music/A_Battle_Cue"));
	static ConstructorHelpers::FObjectFinder<USoundCue> Anomaly1MusicObj(TEXT("/Game/Music/A_Anomaly1_Cue"));
	static ConstructorHelpers::FObjectFinder<USoundCue> Anomaly2MusicObj(TEXT("/Game/Music/A_Anomaly2_Cue"));
	static ConstructorHelpers::FObjectFinder<USoundCue> Daedalus1MusicObj(TEXT("/Game/Music/A_Daedalus1_Cue"));
	static ConstructorHelpers::FObjectFinder<USoundCue> Daedalus2MusicObj(TEXT("/Game/Music/A_Daedalus2_Cue"));

	// Mix references
	static ConstructorHelpers::FObjectFinder<USoundClass> MasterClassObj(TEXT("/Game/Sound/Class_Master.Class_Master"));
	static ConstructorHelpers::FObjectFinder<USoundClass> MusicClassObj(TEXT("/Game/Sound/Class_Music.Class_Music"));
	static ConstructorHelpers::FObjectFinder<USoundClass> EffectsClassObj(TEXT("/Game/Sound/Class_SFX.Class_SFX"));
	static ConstructorHelpers::FObjectFinder<USoundMix> MasterSoundMixObj(TEXT("/Game/Sound/Mix_Master"));
	
	// Alarm sounds
	static ConstructorHelpers::FObjectFinder<USoundCue> TargetWarningSoundObj(TEXT("/Game/Sound/Game/A_TargetWarning"));
	static ConstructorHelpers::FObjectFinder<USoundCue> AttackWarningSoundObj(TEXT("/Game/Sound/Game/A_AttackWarning"));
	static ConstructorHelpers::FObjectFinder<USoundCue> HealthWarningSoundObj(TEXT("/Game/Sound/Game/A_HealthWarning"));
	static ConstructorHelpers::FObjectFinder<USoundCue> HealthWarningHeavySoundObj(TEXT("/Game/Sound/Game/A_HealthWarningHeavy"));
	static ConstructorHelpers::FObjectFinder<USoundCue> CollisionWarningSoundObj(TEXT("/Game/Sound/Game/A_CollisionWarning"));
	static ConstructorHelpers::FObjectFinder<USoundCue> SectorExitWarningSoundObj(TEXT("/Game/Sound/Game/A_SectorExitWarning"));

	// Notifications
	static ConstructorHelpers::FObjectFinder<USoundCue> NegativeClickSoundObj(TEXT("/Game/Sound/Game/A_Negative.A_Negative"));
	static ConstructorHelpers::FObjectFinder<USoundCue> InfoSoundObj(TEXT("/Game/Sound/Game/A_Info.A_Info"));
	static ConstructorHelpers::FObjectFinder<USoundCue> TickSoundObj(TEXT("/Game/Sound/Game/A_LightTick.A_LightTick"));
	static ConstructorHelpers::FObjectFinder<USoundCue> BellSoundObj(TEXT("/Game/Sound/Game/A_Bell.A_Bell"));
	static ConstructorHelpers::FObjectFinder<USoundCue> DeleteSoundObj(TEXT("/Game/Sound/Game/A_Delete.A_Delete"));
	// Sound data
	static ConstructorHelpers::FObjectFinder<USoundCue> NotificationInfoSoundObj(TEXT("/Game/Sound/Game/A_NotificationInfo"));
	static ConstructorHelpers::FObjectFinder<USoundCue> NotificationCombatSoundObj(TEXT("/Game/Sound/Game/A_NotificationCombat"));
	static ConstructorHelpers::FObjectFinder<USoundCue> NotificationQuestSoundObj(TEXT("/Game/Sound/Game/A_NotificationQuest"));
	static ConstructorHelpers::FObjectFinder<USoundCue> NotificationTradingSoundObj(TEXT("/Game/Sound/Game/A_NotificationEconomy"));
	static ConstructorHelpers::FObjectFinder<USoundCue> CrashSoundObj(TEXT("/Game/Sound/Impacts/A_Collision"));
	static ConstructorHelpers::FObjectFinder<USoundCue> MissileWarningSoundObj(TEXT("/Game/Sound/Game/A_MissileWarning"));
	static ConstructorHelpers::FObjectFinder<USoundCue> DockingSoundObj(TEXT("/Game/Sound/Impacts/A_Dock"));

	// Sound class
	MasterSoundClass = MasterClassObj.Object;
	MusicSoundClass = MusicClassObj.Object;
	EffectsSoundClass = EffectsClassObj.Object;
	MasterSoundMix = MasterSoundMixObj.Object;

	// Music track store
	MusicTracks.Reserve(14);
	MusicTracks.Add(NULL);
	MusicTracks.Add(IntroMusicObj.Object);
	MusicTracks.Add(MenuMusicObj.Object);
	MusicTracks.Add(Ambient1MusicObj.Object);
	MusicTracks.Add(Ambient2MusicObj.Object);
	MusicTracks.Add(Travel1MusicObj.Object);
	MusicTracks.Add(Travel2MusicObj.Object);
	MusicTracks.Add(ExplorationMusicObj.Object);
	MusicTracks.Add(DangerMusicObj.Object);
	MusicTracks.Add(CombatMusicObj.Object);
	MusicTracks.Add(BattleMusicObj.Object);
	MusicTracks.Add(Anomaly1MusicObj.Object);
	MusicTracks.Add(Anomaly2MusicObj.Object);
	MusicTracks.Add(Daedalus1MusicObj.Object);
	MusicTracks.Add(Daedalus2MusicObj.Object);

	// Sound references
	TargetWarningSound = TargetWarningSoundObj.Object;
	AttackWarningSound = AttackWarningSoundObj.Object;
	HealthWarningSound = HealthWarningSoundObj.Object;
	HealthWarningHeavySound = HealthWarningHeavySoundObj.Object;
	CollisionWarningSound = CollisionWarningSoundObj.Object;
	SectorExitWarningSound = SectorExitWarningSoundObj.Object;
	// Noficiations
	NotificationInfoSound = NotificationInfoSoundObj.Object;
	NotificationCombatSound = NotificationCombatSoundObj.Object;
	NotificationQuestSound = NotificationQuestSoundObj.Object;
	NotificationTradingSound = NotificationTradingSoundObj.Object;
	CrashSound = CrashSoundObj.Object;
	MissileWarningSound = MissileWarningSoundObj.Object;
	DockingSound = DockingSoundObj.Object;


	// Music sound
	MusicPlayer.Sound = PCIP.CreateDefaultSubobject<UAudioComponent>(this, TEXT("MusicSound"));
	MusicPlayer.Sound->bAutoActivate = false;
	MusicPlayer.Sound->bAutoDestroy = false;
	MusicPlayer.PitchedFade = false;
	MusicPlayer.FadeSpeed = 0.2;
	MusicPlayer.Volume = 0;
	
	// Power sound
	PowerPlayer.Sound = PCIP.CreateDefaultSubobject<UAudioComponent>(this, TEXT("PowerSound"));
	PowerPlayer.Sound->bAutoActivate = false;
	PowerPlayer.Sound->bAutoDestroy = false;
	PowerPlayer.PitchedFade = true;
	PowerPlayer.FadeSpeed = 0.3;
	PowerPlayer.Volume = 0;

	// Engine sound
	EnginePlayer.Sound = PCIP.CreateDefaultSubobject<UAudioComponent>(this, TEXT("EngineSound"));
	EnginePlayer.Sound->bAutoActivate = false;
	EnginePlayer.Sound->bAutoDestroy = false;
	EnginePlayer.PitchedFade = true;
	EnginePlayer.FadeSpeed = 2.0;
	EnginePlayer.Volume = 0;

	// RCS sound
	RCSPlayer.Sound = PCIP.CreateDefaultSubobject<UAudioComponent>(this, TEXT("RCSSound"));
	RCSPlayer.Sound->bAutoActivate = false;
	RCSPlayer.Sound->bAutoDestroy = false;
	RCSPlayer.PitchedFade = true;
	RCSPlayer.FadeSpeed = 5.0;
	RCSPlayer.Volume = 0;

	// Warning sound on targeting
	TargetWarningPlayer.Sound = PCIP.CreateDefaultSubobject<UAudioComponent>(this, TEXT("TargetWarningSound"));
	TargetWarningPlayer.Sound->bAutoActivate = false;
	TargetWarningPlayer.Sound->bAutoDestroy = false;
	TargetWarningPlayer.PitchedFade = false;
	TargetWarningPlayer.FadeSpeed = 10.0f;
	TargetWarningPlayer.Volume = 0;

	// Warning sound on attack
	AttackWarningPlayer.Sound = PCIP.CreateDefaultSubobject<UAudioComponent>(this, TEXT("AttackWarningSound"));
	AttackWarningPlayer.Sound->bAutoActivate = false;
	AttackWarningPlayer.Sound->bAutoDestroy = false;
	AttackWarningPlayer.PitchedFade = false;
	AttackWarningPlayer.FadeSpeed = 10.0f;
	AttackWarningPlayer.Volume = 0;

	// Warning sound on low health
	HealthWarningPlayer.Sound = PCIP.CreateDefaultSubobject<UAudioComponent>(this, TEXT("HealthWarningSound"));
	HealthWarningPlayer.Sound->bAutoActivate = false;
	HealthWarningPlayer.Sound->bAutoDestroy = false;
	HealthWarningPlayer.PitchedFade = false;
	HealthWarningPlayer.FadeSpeed = 1.0f;
	HealthWarningPlayer.Volume = 0;

	// Warning sound on collision
	CollisionWarningPlayer.Sound = PCIP.CreateDefaultSubobject<UAudioComponent>(this, TEXT("CollisionWarningSound"));
	CollisionWarningPlayer.Sound->bAutoActivate = false;
	CollisionWarningPlayer.Sound->bAutoDestroy = false;
	CollisionWarningPlayer.PitchedFade = false;
	CollisionWarningPlayer.FadeSpeed = 10.0f;
	CollisionWarningPlayer.Volume = 0;

	// Warning sound on sector exit
	SectorExitWarningPlayer.Sound = PCIP.CreateDefaultSubobject<UAudioComponent>(this, TEXT("SectorExitWarningSound"));
	SectorExitWarningPlayer.Sound->bAutoActivate = false;
	SectorExitWarningPlayer.Sound->bAutoDestroy = false;
	SectorExitWarningPlayer.PitchedFade = false;
	SectorExitWarningPlayer.FadeSpeed = 10.0f;
	SectorExitWarningPlayer.Volume = 0;

	// Library
	NegativeClickSound = NegativeClickSoundObj.Object;
	InfoSound = InfoSoundObj.Object;
	TickSound = TickSoundObj.Object;
	BellSound = BellSoundObj.Object;
	DeleteSound = DeleteSoundObj.Object;
}


/*----------------------------------------------------
	Gameplay
----------------------------------------------------*/

void UFlareSoundManager::Setup(AFlarePlayerController* Player)
{
	FLOG("UFlareSoundManager::Setup");
	PC = Player;

	// Attach all sounds
	if (Player)
	{
		USceneComponent* RootComponent = Player->GetRootComponent();
		FAttachmentTransformRules AttachRules(EAttachmentRule::KeepWorld, true);

		// Music
		MusicPlayer.Sound->AttachToComponent(RootComponent, AttachRules);
		MusicPlayer.Sound->SetSound(NULL);

		// Engine sounds
		PowerPlayer.Sound->AttachToComponent(RootComponent, AttachRules);
		EnginePlayer.Sound->AttachToComponent(RootComponent, AttachRules);
		RCSPlayer.Sound->AttachToComponent(RootComponent, AttachRules);

		// Warning sounds
		TargetWarningPlayer.Sound->AttachToComponent(RootComponent, AttachRules);
		AttackWarningPlayer.Sound->AttachToComponent(RootComponent, AttachRules);
		HealthWarningPlayer.Sound->AttachToComponent(RootComponent, AttachRules);
		CollisionWarningPlayer.Sound->AttachToComponent(RootComponent, AttachRules);
		SectorExitWarningPlayer.Sound->AttachToComponent(RootComponent, AttachRules);
		TargetWarningPlayer.Sound->SetSound(TargetWarningSound);
		AttackWarningPlayer.Sound->SetSound(AttackWarningSound);
		HealthWarningPlayer.Sound->SetSound(HealthWarningSound);
		CollisionWarningPlayer.Sound->SetSound(CollisionWarningSound);
		SectorExitWarningPlayer.Sound->SetSound(SectorExitWarningSound);
		
		// Device
		FAudioDevice* AudioDevice = Player->GetWorld()->GetAudioDevice();
		if (AudioDevice)
		{
			FCHECK(MasterSoundMix);
			AudioDevice->SetBaseSoundMix(MasterSoundMix);
		}
	}
}

void UFlareSoundManager::SetMusicVolume(int32 Volume)
{
	FLOGV("UFlareSoundManager::SetMusicVolume %d", Volume);

	MusicVolume = FMath::Clamp(Volume / 10.0f, 0.0f, 1.0f);

	// Update
	FAudioDevice* AudioDevice = PC->GetWorld()->GetAudioDevice();
	if (AudioDevice)
	{
		AudioDevice->SetSoundMixClassOverride(MasterSoundMix, MusicSoundClass, MusicVolume, 1.0f, 0.5f, true);
	}
}

void UFlareSoundManager::SetMasterVolume(int32 Volume)
{
	FLOGV("UFlareSoundManager::SetMasterVolume %d", Volume);

	MasterVolume = FMath::Clamp(Volume / 10.0f, 0.0f, 1.0f);

	// Update
	FAudioDevice* AudioDevice = PC->GetWorld()->GetAudioDevice();
	if (AudioDevice)
	{
		FCHECK(MasterSoundMix);
		FCHECK(MasterSoundClass);
		AudioDevice->SetSoundMixClassOverride(MasterSoundMix, MasterSoundClass, MasterVolume, 1.0f, 0.5f, true);
	}
}

void UFlareSoundManager::Update(float DeltaSeconds)
{
	// Update the ship if necessary
	AFlareSpacecraft* PlayerShip = PC->GetShipPawn();
	if (PlayerShip != ShipPawn)
	{
		SetCurrentSpacecraft(PlayerShip);
	}

	// Music management
	if (MusicChanging)
	{
		if (MusicPlayer.Volume > 0)
		{
			UpdatePlayer(MusicPlayer, -DeltaSeconds);
		}
		else
		{
			SetDesiredMusicTrack();
		}
	}
	else if (MusicDesiredTrack == EFlareMusicTrack::None)
	{
		UpdatePlayer(MusicPlayer, -DeltaSeconds);
	}
	else
	{
		UpdatePlayer(MusicPlayer, +DeltaSeconds);
	}

	// Has a ship : update ship sounds
	if (ShipPawn)
	{
		// Engine values
		float RCSAlpha = 0;
		float EngineAlpha = 0;
		int32 RCSCount = 0;
		int32 EngineCount = 0;

		// Check all engines for engine alpha values
		TArray<UActorComponent*> Engines = ShipPawn->GetComponentsByClass(UFlareEngine::StaticClass());
		for (int32 EngineIndex = 0; EngineIndex < Engines.Num(); EngineIndex++)
		{
			UFlareEngine* Engine = Cast<UFlareEngine>(Engines[EngineIndex]);
			if (Engine->IsA(UFlareOrbitalEngine::StaticClass()))
			{
				EngineAlpha += Engine->GetEffectiveAlpha();
				EngineCount++;
			}
			else
			{
				RCSAlpha += Engine->GetEffectiveAlpha();
				RCSCount++;
			}
		}

		// Get player threats
		bool Targeted, FiredUpon, CollidingSoon, ExitingSoon, LowHealth;
		UFlareSimulatedSpacecraft* Threat;
		PC->GetPlayerShipThreatStatus(Targeted, FiredUpon, CollidingSoon, ExitingSoon, LowHealth, Threat);
		UFlareSimulatedSpacecraftDamageSystem* DamageSystem = PlayerShip->GetParent()->GetDamageSystem();
		
		// Update engine sounds
		UpdatePlayer(PowerPlayer,  (!DamageSystem->IsUncontrollable() && !DamageSystem->HasPowerOutage() ? 1 : -1) * DeltaSeconds);
		UpdatePlayer(EnginePlayer, (EngineAlpha > 0 ? EngineAlpha / EngineCount : -1) * DeltaSeconds);
		UpdatePlayer(RCSPlayer,    (RCSAlpha > 0 ? RCSAlpha / RCSCount : -1)          * DeltaSeconds);

		// Update alarms
		bool IsHeavy = (ShipPawn->GetParent()->GetDescription()->Size == EFlarePartSize::L);
		UpdatePlayer(TargetWarningPlayer, (!IsHeavy && !FiredUpon & Targeted ?  1.0f : -1.0f) * DeltaSeconds);
		UpdatePlayer(AttackWarningPlayer, (!IsHeavy &&  FiredUpon ?             1.0f : -1.0f) * DeltaSeconds);
		UpdatePlayer(HealthWarningPlayer, (LowHealth ?                          1.0f : -1.0f) * DeltaSeconds);
		UpdatePlayer(CollisionWarningPlayer, (CollidingSoon ?                   1.0f : -1.0f) * DeltaSeconds);
		UpdatePlayer(SectorExitWarningPlayer, (ExitingSoon ?                    1.0f : -1.0f) * DeltaSeconds);
	}

	// No ship : stop all ship sounds
	else
	{
		UpdatePlayer(PowerPlayer,         -DeltaSeconds);
		UpdatePlayer(EnginePlayer,        -DeltaSeconds);
		UpdatePlayer(RCSPlayer,           -DeltaSeconds);
		UpdatePlayer(TargetWarningPlayer, -DeltaSeconds);
		UpdatePlayer(AttackWarningPlayer, -DeltaSeconds);
		UpdatePlayer(HealthWarningPlayer, -DeltaSeconds);
		UpdatePlayer(CollisionWarningPlayer, -DeltaSeconds);
		UpdatePlayer(SectorExitWarningPlayer, -DeltaSeconds);
	}

	UpdateEffectsVolume(DeltaSeconds);
}

void UFlareSoundManager::RequestMusicTrack(EFlareMusicTrack::Type NewTrack)
{
	FLOGV("UFlareSoundManager::RequestMusicTrack : requested %d", (int32)(NewTrack - EFlareMusicTrack::None));
	if (NewTrack != MusicDesiredTrack)
	{
		FLOG("UFlareSoundManager::RequestMusicTrack : switching");
		MusicDesiredTrack = NewTrack;
		MusicChanging = true;
	}
}


/*----------------------------------------------------
	Internal methods
----------------------------------------------------*/

void UFlareSoundManager::SetCurrentSpacecraft(AFlareSpacecraft* Ship)
{
	ShipPawn = Ship;

	if (Ship)
	{
		// Setup power sound
		FFlareSpacecraftDescription* ShipDescription = Ship->GetParent()->GetDescription();
		PowerPlayer.Sound->SetSound(ShipDescription ? ShipDescription->PowerSound : NULL);

		// Setup orbital engine sound
		FFlareSpacecraftComponentDescription* EngineDescription = Ship->GetOrbitalEngineDescription();
		EnginePlayer.Sound->SetSound(EngineDescription ? EngineDescription->EngineCharacteristics.EngineSound : NULL);

		// Setup RCS sound
		FFlareSpacecraftComponentDescription* RCSDescription = Ship->GetRCSDescription();
		RCSPlayer.Sound->SetSound(RCSDescription ? RCSDescription->EngineCharacteristics.EngineSound : NULL);

		// Setup warning sound
		HealthWarningPlayer.Sound->Sound = ShipDescription->Size == EFlarePartSize::L ? HealthWarningHeavySound : HealthWarningSound;
	}
}

void UFlareSoundManager::SetDesiredMusicTrack()
{
	int32 TrackIndex = (int32)(MusicDesiredTrack - EFlareMusicTrack::None);
	FLOGV("UFlareSoundManager::SetDesiredMusicTrack : starting %d", TrackIndex);
	FCHECK(TrackIndex >= 0 && TrackIndex < MusicTracks.Num());

	MusicPlayer.Sound->SetSound(MusicTracks[TrackIndex]);
	MusicChanging = false;
}

void UFlareSoundManager::UpdateEffectsVolume(float DeltaSeconds)
{
	// Fade in menu transitions
	if (PC)
	{
		if (PC->GetMenuManager()->IsFading())
		{
			EffectsVolume -= DeltaSeconds / PC->GetMenuManager()->GetFadeDuration();
		}
		else
		{
			EffectsVolume += DeltaSeconds / PC->GetMenuManager()->GetFadeDuration();
		}
		EffectsVolume = FMath::Clamp(EffectsVolume, 0.01f, 1.0f);
	}

	// Get device and update
	FAudioDevice* AudioDevice = PC->GetWorld()->GetAudioDevice();
	if (AudioDevice)
	{
		AudioDevice->SetSoundMixClassOverride(MasterSoundMix, EffectsSoundClass, EffectsVolume, 1.0f, 0.0f, true);
	}
}

void UFlareSoundManager::UpdatePlayer(FFlareSoundPlayer& Player, float VolumeDelta)
{
	float NewVolume = FMath::Clamp(Player.Volume + VolumeDelta * Player.FadeSpeed, 0.0f, 1.0f);

	if (NewVolume != Player.Volume)
	{
		if (Player.Volume == 0 && NewVolume != 0)
		{
			Player.Sound->Play();
		}
		else
		{
			Player.Sound->SetVolumeMultiplier(NewVolume);
			if (Player.PitchedFade)
			{
				Player.Sound->SetPitchMultiplier(0.5f + 0.5f * NewVolume);
			}
		}
		Player.Volume = NewVolume;
	}
	else if (Player.Volume == 0)
	{
		Player.Sound->Stop();
	}
}

void UFlareSoundManager::StopPlayer(FFlareSoundPlayer& Player)
{
	Player.Sound->Stop();
	Player.Volume = 0;
}

#undef LOCTEXT_NAMESPACE
