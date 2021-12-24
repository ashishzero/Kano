#include "StringBuilder.h"

char *string_builder_push(String_Builder *builder, uint32_t size)
{
    Assert(size < STRING_BUILDER_BUCKET_SIZE);

    if (builder->write_index + size >= STRING_BUILDER_BUCKET_SIZE)
    {
        auto bucket            = new String_Builder::Bucket;
        builder->write_index   = 0;

        builder->current->next = bucket;
        builder->current       = bucket;
    }

    auto data = &builder->current->data[builder->write_index];
    builder->write_index += size;

    return (char *)data;
}

String string_builder_copy(String_Builder *builder, String src)
{
    char *data = string_builder_push(builder, (uint32_t)src.length + 1);
    memcpy(data, src.data, src.length);
    data[src.length] = 0;
    return String((uint8_t *)data, src.length);
}

String string_builder_copy(String_Builder *builder, uint8_t *src, int64_t size)
{
    return string_builder_copy(builder, String(src, size));
}
