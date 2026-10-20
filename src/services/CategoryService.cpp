#include "services/CategoryService.h"

#include "data/CategoryRepository.h"

namespace talkbox {

CategoryService::CategoryService(CategoryRepository &repository, QObject *parent)
    : QObject(parent)
    , m_repository(repository)
{
}

QString CategoryService::lastError() const
{
    return m_lastError;
}

bool CategoryService::createCategory(const QString &name)
{
    QString error;
    if (!m_repository.create(name, QString(), nullptr, &error))
        return fail(error);

    clearError();
    emit categoriesChanged();
    return true;
}

bool CategoryService::renameCategory(const QString &id, const QString &name)
{
    QString error;
    if (!m_repository.rename(id, name, &error))
        return fail(error);

    clearError();
    emit categoriesChanged();
    return true;
}

bool CategoryService::deleteCategory(const QString &id)
{
    QString error;
    if (!m_repository.remove(id, nullptr, &error))
        return fail(error);

    clearError();
    emit categoriesChanged();
    return true;
}

int CategoryService::snippetCount(const QString &id) const
{
    return m_repository.snippetCount(id);
}

bool CategoryService::fail(const QString &message)
{
    m_lastError = message;
    emit lastErrorChanged();
    return false;
}

void CategoryService::clearError()
{
    if (m_lastError.isEmpty())
        return;
    m_lastError.clear();
    emit lastErrorChanged();
}

} // namespace talkbox
