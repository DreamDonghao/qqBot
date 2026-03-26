#pragma once
#include <QQMessage.hpp>
#include <drogon/HttpController.h>
#include <drogon/utils/coroutine.h>

namespace qqBot {
    class ProcessQQMessages : public drogon::HttpController<ProcessQQMessages> {
    public:
        ProcessQQMessages();
        ~ProcessQQMessages()override;

        METHOD_LIST_BEGIN
        ADD_METHOD_TO(ProcessQQMessages::receiveMessages, "/", drogon::Post);
        METHOD_LIST_END

        drogon::Task<> receiveMessages(drogon::HttpRequestPtr req,
                             std::function<void(const drogon::HttpResponsePtr &)> callback) const;
    private:
    };
} // namespace qqBot
