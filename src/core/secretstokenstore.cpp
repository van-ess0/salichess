// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "secretstokenstore.h"

#include <Secrets/createcollectionrequest.h>
#include <Secrets/deletesecretrequest.h>
#include <Secrets/result.h>
#include <Secrets/secret.h>
#include <Secrets/secretmanager.h>
#include <Secrets/storedsecretrequest.h>
#include <Secrets/storesecretrequest.h>

#include <QtDebug>

using namespace Sailfish::Secrets;

namespace {

// Collection names are unique across all apps, and the encrypted storage
// plugin only accepts up to 31 alphanumeric characters.
const char CollectionName[] = "iogithubvaness0salichess";
const char SecretName[] = "lichessAccessToken";

Secret::Identifier tokenIdentifier()
{
    return Secret::Identifier(QLatin1String(SecretName), QLatin1String(CollectionName),
                              SecretManager::DefaultEncryptedStoragePluginName);
}

bool succeeded(Request &request, const char *what)
{
    request.startRequest();
    request.waitForFinished();
    if (request.result().code() == Result::Succeeded)
        return true;
    qWarning("Sailfish Secrets: %s failed: %s", what, qPrintable(request.result().errorMessage()));
    return false;
}

} // namespace

SecretsTokenStore::SecretsTokenStore()
    : m_manager(new SecretManager)
{
}

SecretsTokenStore::~SecretsTokenStore() = default;

QString SecretsTokenStore::load()
{
    if (!m_manager->isInitialized())
        return QString();

    StoredSecretRequest request;
    request.setManager(m_manager.data());
    request.setIdentifier(tokenIdentifier());
    request.setUserInteractionMode(SecretManager::SystemInteraction);
    request.startRequest();
    request.waitForFinished();
    // No collection or no secret yet simply means "not logged in".
    if (request.result().code() != Result::Succeeded)
        return QString();
    return QString::fromUtf8(request.secret().data());
}

bool SecretsTokenStore::save(const QString &token)
{
    if (!m_manager->isInitialized() || !ensureCollection())
        return false;

    Secret secret(tokenIdentifier());
    secret.setType(Secret::TypeBlob);
    secret.setData(token.toUtf8());

    // Collection secrets are overwritten in place.
    StoreSecretRequest request;
    request.setManager(m_manager.data());
    request.setSecretStorageType(StoreSecretRequest::CollectionSecret);
    request.setUserInteractionMode(SecretManager::SystemInteraction);
    request.setSecret(secret);
    return succeeded(request, "storing the token");
}

void SecretsTokenStore::clear()
{
    if (!m_manager->isInitialized())
        return;

    DeleteSecretRequest request;
    request.setManager(m_manager.data());
    request.setIdentifier(tokenIdentifier());
    request.setUserInteractionMode(SecretManager::SystemInteraction);
    request.startRequest();
    request.waitForFinished(); // "not found" is fine
}

bool SecretsTokenStore::ensureCollection()
{
    CreateCollectionRequest request;
    request.setManager(m_manager.data());
    request.setCollectionName(QLatin1String(CollectionName));
    request.setAccessControlMode(SecretManager::OwnerOnlyMode);
    request.setCollectionLockType(CreateCollectionRequest::DeviceLock);
    request.setDeviceLockUnlockSemantic(SecretManager::DeviceLockKeepUnlocked);
    request.setStoragePluginName(SecretManager::DefaultEncryptedStoragePluginName);
    request.setEncryptionPluginName(SecretManager::DefaultEncryptedStoragePluginName);
    request.setUserInteractionMode(SecretManager::SystemInteraction);
    request.startRequest();
    request.waitForFinished();

    if (request.result().code() == Result::Succeeded
            || request.result().errorCode() == Result::CollectionAlreadyExistsError)
        return true;
    qWarning("Sailfish Secrets: creating the collection failed: %s",
             qPrintable(request.result().errorMessage()));
    return false;
}
