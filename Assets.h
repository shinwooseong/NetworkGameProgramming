#pragma once
#include <windows.h>
#include <string>

// All asset names live here. Paths are relative to the executable.
namespace Assets {
inline std::wstring Path(const wchar_t* relativePath) {
    wchar_t executable[32768] = {};
    const DWORD length = GetModuleFileNameW(nullptr, executable, 32768);
    if (length == 0 || length >= 32768) return {};
    std::wstring directory(executable, length);
    directory.resize(directory.find_last_of(L"\\/") + 1);
    return directory + L"resources\\" + relativePath;
}


// 폭파 이미지
inline std::wstring Explosion_1_0001() { return Path(L"images\\Explosion_1_0001.png"); }
inline std::wstring Explosion_1_0002() { return Path(L"images\\Explosion_1_0002.png"); }
inline std::wstring Explosion_1_0003() { return Path(L"images\\Explosion_1_0003.png"); }
inline std::wstring Explosion_1_0004() { return Path(L"images\\Explosion_1_0004.png");}


// 충격 이미지
inline std::wstring Hit_1_0001() { return Path(L"images\\Hit_1_0001.png"); }
inline std::wstring Hit_1_0002() { return Path(L"images\\Hit_1_0002.png"); }
inline std::wstring Hit_1_0003() { return Path(L"images\\Hit_1_0003.png"); }
inline std::wstring Hit_1_0004() { return Path(L"images\\Hit_1_0004.png"); }

// 아이템 슬롯 이미지
inline std::wstring Itemslot() { return Path(L"images\\Itemslot.png"); }

// 배경 이미지 & 배경 사운드
inline std::wstring background_1() { return Path(L"images\\background_1.png"); }
inline std::wstring background_2() { return Path(L"images\\background_2.png"); }
inline std::wstring background_bgm() { return Path(L"sounds\\background_bgm.wav"); }

// 게임 리소스 이미지&사운드
inline std::wstring bulletImage() { return Path(L"images\\bulletImage.png"); }
inline std::wstring enemyImage() { return Path(L"images\\enemyImage.png"); }
inline std::wstring enemyImage2() { return Path(L"images\\enemyImage2.png"); }
inline std::wstring enemyImage3() { return Path(L"images\\enemyImage3.png"); }
inline std::wstring fadechange() { return Path(L"sounds\\fadechange.wav"); }
inline std::wstring fogImage() { return Path(L"images\\fog.png"); }
inline std::wstring fogSound() { return Path(L"sounds\\fog.wav"); }
inline std::wstring gameendbg() { return Path(L"images\\gameendbg.jpg"); }
inline std::wstring gameover() { return Path(L"images\\gameover.png"); }
inline std::wstring gamestart() { return Path(L"images\\gamestart.png"); }
inline std::wstring gamestartbg() { return Path(L"images\\gamestartbg.jpg"); }
inline std::wstring heal() { return Path(L"sounds\\heal.wav"); }
inline std::wstring hit() { return Path(L"sounds\\hit.wav"); }
inline std::wstring hurt() { return Path(L"sounds\\hurt.wav"); }
inline std::wstring item_effect_0() { return Path(L"images\\item_effect_0.png"); }
inline std::wstring item_effect_1() { return Path(L"images\\item_effect_1.png"); }
inline std::wstring item_effect_2() { return Path(L"images\\item_effect_2.png"); }
inline std::wstring item_effect_3() { return Path(L"images\\item_effect_3.png"); }
inline std::wstring item_fog() { return Path(L"images\\item_fog.png"); }
inline std::wstring item_heal() { return Path(L"images\\item_heal.png"); }
inline std::wstring item_posion() { return Path(L"images\\item_posion.png"); }
inline std::wstring item_rebirth() { return Path(L"images\\item_rebirth.png"); }
inline std::wstring item_speed() { return Path(L"images\\item_speed.png"); }
inline std::wstring lose() { return Path(L"sounds\\lose.wav"); }
inline std::wstring meteor() { return Path(L"sounds\\meteor.wav"); }
inline std::wstring meteor1() { return Path(L"images\\meteor1.png"); }
inline std::wstring meteor2() { return Path(L"images\\meteor2.png"); }
inline std::wstring meteor3() { return Path(L"images\\meteor3.png"); }
inline std::wstring playerImage() { return Path(L"images\\playerImage.png"); }
inline std::wstring posion() { return Path(L"sounds\\posion.wav"); }
inline std::wstring redcircle() { return Path(L"images\\redcircle.png"); }
inline std::wstring revive() { return Path(L"sounds\\revive.wav"); }
inline std::wstring revive1() { return Path(L"images\\revive1.png"); }
inline std::wstring revive2() { return Path(L"images\\revive2.png"); }
inline std::wstring revive3() { return Path(L"images\\revive3.png"); }
inline std::wstring shot() { return Path(L"sounds\\shot.wav"); }
inline std::wstring speed() { return Path(L"sounds\\speed.wav"); }
inline std::wstring startscreen_bgm() { return Path(L"sounds\\startscreen_bgm.wav"); }
inline std::wstring text() { return Path(L"images\\text.png"); }
inline std::wstring win() { return Path(L"sounds\\win.wav"); }
inline std::wstring youwin() { return Path(L"images\\youwin.png"); }
}

