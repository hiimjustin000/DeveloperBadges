#include <Geode/Geode.hpp>
#include <Geode/utils/web.hpp>

using namespace geode::prelude;

enum class BadgeType {
    None,
    ModDeveloper,
    VerifiedDeveloper,
    IndexStaff,
    LeadDeveloper
};

struct DeveloperBadge {
    int id;
    std::string name;
    BadgeType badge;
};

std::vector<DeveloperBadge> DEVELOPER_BADGES;
bool TRIED_LOADING = false;

#define BADGE_URL "https://raw.githubusercontent.com/hiimjustin000/developer-badges/refs/heads/master/badges.json"

#define PROPERTY_OR_DEFAULT(obj, prop, isFunc, asFunc, def) (obj.contains(prop) && obj[prop].isFunc() ? obj[prop].asFunc() : def)

void showBadgeInfo(std::string username, CCMenuItemSpriteExtra* badgeButton) {
    auto badge = BadgeType::None;
    auto badgeRect = static_cast<CCSprite*>(badgeButton->getNormalImage())->getTextureRect();
    auto sfc = CCSpriteFrameCache::sharedSpriteFrameCache();
    if (badgeRect == sfc->spriteFrameByName("badge01.png"_spr)->getRect()) badge = BadgeType::ModDeveloper;
    else if (badgeRect == sfc->spriteFrameByName("badge02.png"_spr)->getRect()) badge = BadgeType::VerifiedDeveloper;
    else if (badgeRect == sfc->spriteFrameByName("badge03.png"_spr)->getRect()) badge = BadgeType::IndexStaff;
    else if (badgeRect == sfc->spriteFrameByName("badge04.png"_spr)->getRect()) badge = BadgeType::LeadDeveloper;

    auto badgeName = "";
    switch (badge) {
        case BadgeType::ModDeveloper: badgeName = "Mod Developer"; break;
        case BadgeType::VerifiedDeveloper: badgeName = "Verified Developer"; break;
        case BadgeType::IndexStaff: badgeName = "Index Staff"; break;
        case BadgeType::LeadDeveloper: badgeName = "Lead Developer"; break;
        default: badgeName = ""; break;
    }

    auto badgeDesc = "";
    switch (badge) {
        case BadgeType::ModDeveloper: badgeDesc = "is a <ca>mod developer</c> for <cy>Geode</c>.\n"
            "They have created mods that are available on the <cy>Geode mod index</c>.\n"
            "They will have to have new mods and mod updates approved by the <cd>index staff</c>"; break;
        case BadgeType::VerifiedDeveloper: badgeDesc = "is a <cp>verified mod developer</c> for <cy>Geode</c>.\n"
            "They can update their mods on the <cy>Geode mod index</c> without the need for approval.\n"
            "They will still have to have new mods approved by the <cd>index staff</c>"; break;
        case BadgeType::IndexStaff: badgeDesc = "is a member of the <cd>index staff</c> for <cy>Geode</c>.\n"
            "They can approve or reject mods uploaded to the <cy>Geode mod index</c>"; break;
        case BadgeType::LeadDeveloper: badgeDesc = "is a <co>lead developer</c> of <cy>Geode</c>.\n"
            "They are part of the main development team and have significant contributions to the <cy>Geode ecosystem</c>"; break;
        default: badgeDesc = ""; break;
    }

    FLAlertLayer::create(badgeName, fmt::format("<cg>{}</c> {}.", username, badgeDesc), "OK")->show();
}

ccColor3B getCommentColor(BadgeType badge) {
    switch (badge) {
        case BadgeType::ModDeveloper: return { 163, 121, 192 };
        case BadgeType::VerifiedDeveloper: return { 202, 134, 159 };
        case BadgeType::IndexStaff: return { 223, 143, 143 };
        case BadgeType::LeadDeveloper: return { 225, 178, 155 };
        default: return { 255, 255, 255 };
    }
}

#include <Geode/modify/MenuLayer.hpp>
class $modify(DBMenuLayer, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) return false;

        if (TRIED_LOADING) return true;

        static std::optional<web::WebTask> task = std::nullopt;
        task = web::WebRequest().get(BADGE_URL).map([](web::WebResponse* res) {
            if (res->ok()) {
                auto str = res->string().value();
                std::string error;
                auto json = matjson::parse(str, error).value_or(matjson::Array());
                if (!error.empty()) log::error("Failed to parse developer badges: {}", error);
                if (json.is_array()) for (auto& badge : json.as_array()) {
                    auto id = PROPERTY_OR_DEFAULT(badge, "id", is_number, as_int, 0);
                    if (id > 0) DEVELOPER_BADGES.push_back({
                        .id = id,
                        .name = PROPERTY_OR_DEFAULT(badge, "name", is_string, as_string, ""),
                        .badge = (BadgeType)PROPERTY_OR_DEFAULT(badge, "badge", is_number, as_int, 0)
                    });
                }
            }
            else Loader::get()->queueInMainThread([] {
                Notification::create("Developer Badges load failed", NotificationIcon::Error)->show();
            });

            task = std::nullopt;
            return *res;
        });

        TRIED_LOADING = true;

        return true;
    }
};

#include <Geode/modify/ProfilePage.hpp>
class $modify(DBProfilePage, ProfilePage) {
    void loadPageFromUserInfo(GJUserScore* score) {
        ProfilePage::loadPageFromUserInfo(score);

        auto usernameMenu = m_mainLayer->getChildByID("username-menu");
        if (!usernameMenu) return;

        auto it = std::find_if(DEVELOPER_BADGES.begin(), DEVELOPER_BADGES.end(), [score](const DeveloperBadge& badge) {
            return badge.id == score->m_accountID && badge.badge != BadgeType::None;
        });
        if (it == DEVELOPER_BADGES.end()) return;

        auto badge = *it;
        auto badgeButton = CCMenuItemSpriteExtra::create(
            CCSprite::createWithSpriteFrameName(fmt::format("badge{:02}.png"_spr, (int)badge.badge).c_str()),
            this,
            menu_selector(DBProfilePage::onBadge)
        );
        badgeButton->setID("developer-badge"_spr);
        usernameMenu->addChild(badgeButton);
        usernameMenu->updateLayout();
    }

    void onBadge(CCObject* sender) {
        showBadgeInfo(m_score->m_userName, static_cast<CCMenuItemSpriteExtra*>(sender));
    }
};

#include <Geode/modify/CommentCell.hpp>
class $modify(DBCommentCell, CommentCell) {
    void loadFromComment(GJComment* comment) {
        CommentCell::loadFromComment(comment);

        auto usernameMenu = m_mainLayer->getChildByIDRecursive("username-menu");
        if (!usernameMenu) return;

        auto it = std::find_if(DEVELOPER_BADGES.begin(), DEVELOPER_BADGES.end(), [comment](const DeveloperBadge& badge) {
            return badge.id == comment->m_accountID && badge.badge != BadgeType::None;
        });
        if (it == DEVELOPER_BADGES.end()) return;

        auto badge = *it;
        auto badgeSprite = CCSprite::createWithSpriteFrameName(fmt::format("badge{:02}.png"_spr, (int)badge.badge).c_str());
        badgeSprite->setScale(0.7f);
        auto badgeButton = CCMenuItemSpriteExtra::create(
            badgeSprite,
            this,
            menu_selector(DBCommentCell::onBadge)
        );
        badgeButton->setID("developer-badge"_spr);
        usernameMenu->addChild(badgeButton);
        usernameMenu->updateLayout();

        if (m_comment->m_modBadge >= 1 || m_comment->m_levelID == 0) return;

        if (auto commentTextLabel = static_cast<CCLabelBMFont*>(m_mainLayer->getChildByID("comment-text-label")))
            commentTextLabel->setColor(getCommentColor(badge.badge));

        if (auto commentEmojisLabel = static_cast<CCLabelBMFont*>(m_mainLayer->getChildByID("thesillydoggo.comment_emojis/comment-text-label")))
            commentEmojisLabel->setColor(getCommentColor(badge.badge));

        if (auto commentTextArea = static_cast<TextArea*>(m_mainLayer->getChildByID("comment-text-area")))
            commentTextArea->colorAllCharactersTo(getCommentColor(badge.badge));

        if (auto commentEmojisArea = static_cast<CCLabelBMFont*>(m_mainLayer->getChildByID("thesillydoggo.comment_emojis/comment-text-area"))) {
            for (auto i = 0; i < commentEmojisArea->getChildrenCount(); i++) {
                if (auto child = typeinfo_cast<CCLabelBMFont*>(commentEmojisArea->getChildren()->objectAtIndex(i)))
                    child->setColor(getCommentColor(badge.badge));
            }
        }
    }

    void onBadge(CCObject* sender) {
        showBadgeInfo(m_comment->m_userName, static_cast<CCMenuItemSpriteExtra*>(sender));
    }
};
