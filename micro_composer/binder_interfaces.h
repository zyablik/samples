#pragma once

#define MICRO_COMPOSER_SERVICE_NAME "micro-composer-service"

enum IMicroComposer {
    GET_WINDOW_TRANSACTION
};

enum INativeWindow {
    QUERY_TRANSACTION,
    PERFORM_TRANSACTION,
    QUEUE_BUFFER_TRANSACTION,
    DEQUEUE_BUFFER_TRANSACTION,
};
