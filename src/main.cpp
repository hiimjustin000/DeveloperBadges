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

void showBadgeInfo(std::string username, CCObject* badgeButton) {
    auto badgeName = "";
    auto badgeDesc = "";
    switch ((BadgeType)badgeButton->getTag()) {
        case BadgeType::ModDeveloper:
            badgeName = "Mod Developer";
            badgeDesc = "is a <ca>mod developer</c> for <cy>Geode</c>.\n"
            "They have created mods that are available on the <cy>Geode mod index</c>.\n"
            "They will have to have new mods and mod updates approved by the <cd>index staff</c>";
            break;
        case BadgeType::VerifiedDeveloper:
            badgeName = "Verified Developer";
            badgeDesc = "is a <cp>verified mod developer</c> for <cy>Geode</c>.\n"
            "They can update their mods on the <cy>Geode mod index</c> without the need for approval.\n"
            "They will still have to have new mods approved by the <cd>index staff</c>";
            break;
        case BadgeType::IndexStaff:
            badgeName = "Index Staff";
            badgeDesc = "is a member of the <cd>index staff</c> for <cy>Geode</c>.\n"
            "They can approve or reject mods uploaded to the <cy>Geode mod index</c>";
            break;
        case BadgeType::LeadDeveloper:
            badgeName = "Lead Developer";
            badgeDesc = "is a <co>lead developer</c> of <cy>Geode</c>.\n"
            "They are part of the main development team and have significant contributions to the <cy>Geode ecosystem</c>";
            break;
        default:
            badgeName = "";
            badgeDesc = "";
            break;
    }

    FLAlertLayer::create(badgeName, fmt::format("<cg>{}</c> {}.", username, badgeDesc), "OK")->show();
}

ccColor3B getCommentColor(BadgeType badge) {
    switch (badge) {
        case BadgeType::ModDeveloper: return Mod::get()->getSettingValue<ccColor3B>("mod-developer-color");
        case BadgeType::VerifiedDeveloper: return Mod::get()->getSettingValue<ccColor3B>("verified-developer-color");
        case BadgeType::IndexStaff: return Mod::get()->getSettingValue<ccColor3B>("index-staff-color");
        case BadgeType::LeadDeveloper: return Mod::get()->getSettingValue<ccColor3B>("lead-developer-color");
        default: return ccColor3B { 255, 255, 255 };
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
                Notification::create("Developer Badges Load Failed", NotificationIcon::Error)->show();
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
    struct Fields {
        bool m_hasBadge;
    };

    void loadPageFromUserInfo(GJUserScore* score) {
        ProfilePage::loadPageFromUserInfo(score);

        if (m_fields->m_hasBadge) return;

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
        badgeButton->setTag((int)badge.badge);
        usernameMenu->addChild(badgeButton);
        usernameMenu->updateLayout();

        m_fields->m_hasBadge = true;
    }

    void onBadge(CCObject* sender) {
        showBadgeInfo(m_score->m_userName, sender);
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
        badgeButton->setTag((int)badge.badge);
        usernameMenu->addChild(badgeButton);
        usernameMenu->updateLayout();

        if (m_comment->m_modBadge >= 1 || m_comment->m_levelID == 0) return;
        if (badge.badge == BadgeType::ModDeveloper && !Mod::get()->getSettingValue<bool>("mod-developer-color-toggle")) return;
        if (badge.badge == BadgeType::VerifiedDeveloper && !Mod::get()->getSettingValue<bool>("verified-developer-color-toggle")) return;
        if (badge.badge == BadgeType::IndexStaff && !Mod::get()->getSettingValue<bool>("index-staff-color-toggle")) return;
        if (badge.badge == BadgeType::LeadDeveloper && !Mod::get()->getSettingValue<bool>("lead-developer-color-toggle")) return;

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
        showBadgeInfo(m_comment->m_userName, sender);
    }
};
