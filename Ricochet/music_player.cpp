#include "music_player.hpp"

MusicPlayer::MusicPlayer()
	: m_volume(24.f)
{
	m_filenames[MusicThemes::kMenuTheme] = "Media/Music/MenuTheme.ogg";
	m_filenames[MusicThemes::kMissionTheme] = "Media/Music/MissionTheme.ogg";
}

void MusicPlayer::Play(MusicThemes theme)
{
	std::string filename = m_filenames[theme];

	if (!m_music.openFromFile(filename))
		throw std::runtime_error("Music " + filename + " could not be loaded.");

	m_music.setVolume(m_volume);
	m_music.setLooping(true);
	m_music.setRelativeToListener(false);
	m_music.play();
	sf::SoundSource::Status status = m_music.getStatus();
	if (status != sf::SoundSource::Status::Playing) {
	}
	else {
	}
}

void MusicPlayer::Stop()
{
	m_music.stop();
}

void MusicPlayer::SetVolume(float volume)
{
	m_volume = volume;
}

void MusicPlayer::SetPaused(bool paused)
{
	if (paused)
		m_music.pause();
	else
		m_music.play();
}

MusicPlayer::~MusicPlayer() {
}
