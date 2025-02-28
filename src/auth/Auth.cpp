#include "Auth.hpp"
#include "Pam.hpp"
#include "Fingerprint.hpp"
#include "../config/ConfigManager.hpp"
#include "../core/hyprlock.hpp"
#include "src/helpers/Log.hpp"

#include <hyprlang.hpp>
#include <memory>

CAuth::CAuth() {
    static auto* const PENABLEPAM = (Hyprlang::INT* const*)g_pConfigManager->getValuePtr("auth:pam:enabled");
    if (**PENABLEPAM)
        m_vImpls.push_back(std::make_shared<CPam>());
    static auto* const PENABLEFINGERPRINT = (Hyprlang::INT* const*)g_pConfigManager->getValuePtr("auth:fingerprint:enabled");
    if (**PENABLEFINGERPRINT)
        m_vImpls.push_back(std::make_shared<CFingerprint>());

    RASSERT(!m_vImpls.empty(), "At least one authentication method must be enabled!");
}

void CAuth::start() {
    for (const auto& i : m_vImpls) {
        i->init();
    }
}

void CAuth::submitInput(const std::string& input) {
    for (const auto& i : m_vImpls) {
        i->handleInput(input);
    }
}

bool CAuth::checkWaiting() {
    for (const auto& i : m_vImpls) {
        if (i->checkWaiting())
            return true;
    }

    return false;
}

const std::string& CAuth::getCurrentFailText() {
    return m_sCurrentFail.failText;
}

std::optional<std::string> CAuth::getFailText(eAuthImplementations implType) {
    for (const auto& i : m_vImpls) {
        if (i->getImplType() == implType)
            return i->getLastFailText();
    }
    return std::nullopt;
}

std::optional<std::string> CAuth::getPrompt(eAuthImplementations implType) {
    for (const auto& i : m_vImpls) {
        if (i->getImplType() == implType)
            return i->getLastPrompt();
    }
    return std::nullopt;
}

size_t CAuth::getFailedAttempts() {
    return m_sCurrentFail.failedAttempts;
}

std::shared_ptr<IAuthImplementation> CAuth::getImpl(eAuthImplementations implType) {
    for (const auto& i : m_vImpls) {
        if (i->getImplType() == implType)
            return i;
    }

    return nullptr;
}

void CAuth::terminate() {
    for (const auto& i : m_vImpls) {
        i->terminate();
    }
}

static void passwordFailCallback(std::shared_ptr<CTimer> self, void* data) {
    g_pAuth->m_bDisplayFailText = true;

    g_pHyprlock->clearPasswordBuffer();

    g_pHyprlock->enqueueForceUpdateTimers();

    g_pHyprlock->renderAllOutputs();
}

static void passwordUnlockCallback(std::shared_ptr<CTimer> self, void* data) {
    g_pHyprlock->unlock();
}

void CAuth::enqueueFail(const std::string& failText, eAuthImplementations implType) {
    m_sCurrentFail.failText   = failText;
    m_sCurrentFail.failSource = implType;
    m_sCurrentFail.failedAttempts++;

    Debug::log(LOG, "Failed attempts: {}", m_sCurrentFail.failedAttempts);

    g_pHyprlock->addTimer(std::chrono::milliseconds(0), passwordFailCallback, nullptr);
}

void CAuth::enqueueUnlock() {
    g_pHyprlock->addTimer(std::chrono::milliseconds(0), passwordUnlockCallback, nullptr);
}
