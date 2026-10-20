#pragma once

#include <QString>

namespace talkbox {

/// A content category. "全部" is a virtual UI filter and is never stored, and
/// "未分类" is the absence of a category (snippets.category_id IS NULL) rather
/// than a row: the user decides which categories exist.
struct Category
{
    QString id;
    QString name;
    QString colorKey; // palette token understood by the UI (e.g. "blue")
    int sortOrder = 0;
    QString createdAt;
};

} // namespace talkbox
