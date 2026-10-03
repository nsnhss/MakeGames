// event_system_actions_audio.cpp — 音频动作（从 event_system.cpp 拆分，D1）
#include "scene/event_system.h"
#include "audio/audio_manager.h"

namespace mg {
namespace scene {

void EventSystem::ExecAudioActions(const Action& action) {
    switch (action.cmd) {
        case ActionCmd::PlaySound: {
            AudioManager::Get().PlaySE(action.text);
            break;
        }
        case ActionCmd::PlayBGM: {
            AudioManager::Get().PlayBGM(action.text, 1.0f, action.float_value);
            break;
        }
        case ActionCmd::StopBGM: {
            AudioManager::Get().StopBGM(action.float_value);
            break;
        }
        case ActionCmd::PlayBGS: {
            AudioManager::Get().PlayBGS(action.text, 1.0f, action.float_value);
            break;
        }
        case ActionCmd::StopBGS: {
            AudioManager::Get().StopBGS(action.float_value);
            break;
        }
        case ActionCmd::PlayME: {
            AudioManager::Get().PlayME(action.text);
            break;
        }
        default:
            break;
    }
}

} // namespace scene
} // namespace mg
