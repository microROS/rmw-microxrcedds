#include "utils.h"

#include "rmw/allocators.h"

static const char* const ros_topic_prefix = "rt";

void custompublisher_clear(CustomPublisher* publisher);
void customsubscription_clear(CustomSubscription* subscription);

void rmw_delete(void* rmw_allocated_ptr)
{
    rmw_free(rmw_allocated_ptr);
    rmw_allocated_ptr = NULL;
}

void rmw_node_delete(rmw_node_t* node)
{
    if (node->namespace_)
    {
        rmw_delete((char*)node->namespace_);
    }
    if (node->name)
    {
        rmw_delete((char*)node->name);
    }
    if (node->implementation_identifier)
    {
        node->implementation_identifier = NULL;
    }
    if (node->data)
    {
        customnode_clear((CustomNode*)node->data);
        node->data = NULL;
    }

    rmw_node_free(node);
    node = NULL;
}

void rmw_publisher_delete(rmw_publisher_t* publisher)
{
    if (publisher->implementation_identifier)
    {
        publisher->implementation_identifier = NULL;
    }
    if (publisher->topic_name)
    {
        rmw_delete((char*)publisher->topic_name);
    }
    if (publisher->data)
    {
        custompublisher_clear((CustomPublisher*)publisher->data);
        publisher->data = NULL;
    }
    rmw_delete(publisher);
}

void custompublisher_clear(CustomPublisher* publisher)
{
    if (publisher)
    {
        memset(&publisher->publisher_id, 0, sizeof(mrObjectId));
        memset(&publisher->datawriter_id, 0, sizeof(mrObjectId));
        memset(&publisher->topic_id, 0, sizeof(mrObjectId));
        publisher->publisher_gid.implementation_identifier = NULL;
        memset(&publisher->publisher_gid.data, 0, RMW_GID_STORAGE_SIZE);
        publisher->type_support = NULL;
    }
}

void publishers_clear(CustomPublisher publishers[static MAX_PUBLISHERS_X_NODE])
{
    for (size_t i = 0; i < MAX_PUBLISHERS_X_NODE; i++)
    {
        custompublisher_clear(&publishers[i]);
    }
}

void rmw_subscription_delete(rmw_subscription_t* subscriber)
{
    if (subscriber->implementation_identifier)
    {
        subscriber->implementation_identifier = NULL;
    }
    if (subscriber->topic_name)
    {
        rmw_delete((char*)subscriber->topic_name);
    }
    if (subscriber->data)
    {
        customsubscription_clear((CustomSubscription*)subscriber->data);
        subscriber->data = NULL;
    }
    rmw_delete(subscriber);
}

void customsubscription_clear(CustomSubscription* subscription)
{
    if (subscription)
    {
        memset(&subscription->subscriber_id, 0, sizeof(mrObjectId));
        memset(&subscription->datareader_id, 0, sizeof(mrObjectId));
        memset(&subscription->topic_id, 0, sizeof(mrObjectId));
        subscription->subscription_gid.implementation_identifier = NULL;
        memset(&subscription->subscription_gid.data, 0, RMW_GID_STORAGE_SIZE);
        subscription->type_support = NULL;
    }
}

void subscriptions_clear(CustomSubscription subscriptions[static MAX_SUBSCRIPTIONS_X_NODE])
{
    for (size_t i = 0; i < MAX_SUBSCRIPTIONS_X_NODE; i++)
    {
        customsubscription_clear(&subscriptions[i]);
    }
}

void customnode_clear(CustomNode* node)
{
    if (node)
    {
        publishers_clear(node->publisher_info);
        free_mem_pool(&node->publisher_mem);
        subscriptions_clear(node->subscription_info);
        free_mem_pool(&node->subscription_mem);
    }
}

int build_participant_xml(size_t domain_id, const char* participant_name, char xml[], size_t buffer_size)
{
    static const char* const format =
        "<profiles><participant "
        "profile_name=\"participant_profile\"><rtps><builtin><leaseDuration><durationbyname>INFINITE</durationbyname></"
        "leaseDuration><domainId>%ld</domainId></builtin><name>%s</name></rtps></participant></profiles>";
    int ret = 0;
    if (buffer_size >= (strlen(format) - 5 + strlen(participant_name + sizeof(size_t))))
    {
        ret = sprintf(xml, format, domain_id, participant_name);
    }
    return ret;
}

int build_publisher_xml(const char* publisher_name, char xml[], size_t buffer_size)
{
    static const char* const format = "<publisher name=\"%s\">";
    int ret                         = 0;
    if (buffer_size >= (strlen(format) - 2 + strlen(publisher_name)))
    {
        ret = sprintf(xml, format, publisher_name);
    }
    return ret;
}

int build_subscriber_xml(const char* subscriber_name, char xml[], size_t buffer_size)
{
    static const char* const format = "<subscriber name=\"%s\">";
    int ret                         = 0;
    if (buffer_size >= (strlen(format) - 2 + strlen(subscriber_name)))
    {
        ret = sprintf(xml, format, subscriber_name);
    }
    return ret;
}

int generate_name(const mrObjectId* id, char name[], size_t buffer_size)
{
    static const char* const format = "%hu_%hi";
    int ret                         = 0;
    if (buffer_size >= (strlen(format) - 6 + sizeof(id->id) + sizeof(id->type)))
    {
        ret = sprintf(name, format, id->id, id->type);
    }
    return ret;
}

int generate_type_name(const message_type_support_callbacks_t* members, const char* sep, char type_name[],
                       size_t buffer_size)
{
    static const char* const format = "%s::%s::dds_::%s_";
    int ret                         = 0;
    if (buffer_size >=
        (strlen(format) - 6 + strlen(members->package_name_) + strlen(sep) + strlen(members->message_name_)))
    {
        ret = sprintf(type_name, format, members->package_name_, sep, members->message_name_);
    }
    return ret;
}

int build_topic_xml(const char* topic_name, const message_type_support_callbacks_t* members,
                    const rmw_qos_profile_t* qos_policies, char xml[], size_t buffer_size)
{
    static const char* const format = "<dds><topic><name>%s</name><dataType>%s</dataType></topic></dds>";
    int ret                         = 0;
    static char type_name_buffer[50];
    if (generate_type_name(members, "msg", type_name_buffer, sizeof(type_name_buffer)))
    {
        char full_topic_name[strlen(topic_name) + strlen(ros_topic_prefix)];
        full_topic_name[0] = '\0';
        if (!qos_policies->avoid_ros_namespace_conventions)
        {
            strcat(full_topic_name, ros_topic_prefix);
        }
        strcat(full_topic_name, topic_name);

        if (buffer_size >= (strlen(format) - 4 + strlen(full_topic_name) + strlen(type_name_buffer)))
        {
            ret = sprintf(xml, format, full_topic_name, type_name_buffer);
        }
    }
    return ret;
}

int build_xml(const char* format, const char* topic_name, const message_type_support_callbacks_t* members,
              const rmw_qos_profile_t* qos_policies, char xml[], size_t buffer_size)
{
    int ret = 0;
    static char type_name_buffer[50];
    if (generate_type_name(members, "msg", type_name_buffer, sizeof(type_name_buffer)))
    {
        char full_topic_name[strlen(topic_name) + strlen(ros_topic_prefix)];
        full_topic_name[0] = '\0';
        if (!qos_policies->avoid_ros_namespace_conventions)
        {
            strcat(full_topic_name, ros_topic_prefix);
        }
        strcat(full_topic_name, topic_name);

        if (buffer_size >= (strlen(format) - 4 + strlen(full_topic_name) + strlen(type_name_buffer)))
        {
            ret = sprintf(xml, format, full_topic_name, type_name_buffer);
        }
    }
    return ret;
}
int build_datawriter_xml(const char* topic_name, const message_type_support_callbacks_t* members,
                         const rmw_qos_profile_t* qos_policies, char xml[], size_t buffer_size)
{
    static const char* const format =
        "<profiles><publisher "
        "profile_name=\"rmw_micrortps_publisher\"><topic><kind>NO_KEY</kind><name>%s</"
        "name><dataType>%s</dataType><historyQos><kind>KEEP_LAST</kind><depth>5</depth></"
        "historyQos><durability><kind>TRANSIENT_LOCAL</kind></durability></topic></publisher></profiles>";
    return build_xml(format, topic_name, members, qos_policies, xml, buffer_size);
}

int build_datareader_xml(const char* topic_name, const message_type_support_callbacks_t* members,
                         const rmw_qos_profile_t* qos_policies, char xml[], size_t buffer_size)
{
    static const char* const format =
        "<profiles><subscriber "
        "profile_name=\"rmw_micrortps_subscriber\"><topic><kind>NO_KEY</kind><name>%s</"
        "name><dataType>%s</dataType><historyQos><kind>KEEP_LAST</kind><depth>5</depth></"
        "historyQos><durability><kind>TRANSIENT_LOCAL</kind></durability></topic></subscriber></profiles>";
    return build_xml(format, topic_name, members, qos_policies, xml, buffer_size);
}