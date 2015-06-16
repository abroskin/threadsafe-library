#ifndef HASHMAP_H
#define HASHMAP_H

#include <string>
#include <mutex>
#include <atomic>
#include <memory>
#include <tuple>
#include <cmath>

template <typename Key, typename Value, typename Hash = std::hash<Key>, typename Comp = std::equal_to<Key>>
class Hashmap
{
private:
    struct BucketItem
    {
        const std::pair<Key, Value> data;
        std::unique_ptr<BucketItem> next;
    };

    struct Bucket
    {
        Bucket():is_obsolete(false)
        {}

        std::mutex lock;
        std::unique_ptr<BucketItem> first;
        bool is_obsolete;
    };

    struct BucketsAccess
    {
        size_t bucket_count;
        std::unique_ptr<Bucket[]> m_buckets;
    };

public:
    Hashmap( const size_t bucket_count ):
        m_hash( Hash() ),
        m_comp( Comp() ),
        m_size( 0u )
    {
        m_buckets_access = std::shared_ptr<BucketsAccess>( new BucketsAccess );
        m_buckets_access->bucket_count = bucket_count;
        m_buckets_access->m_buckets = std::unique_ptr<Bucket[]>( new Bucket[bucket_count] );
    }

    bool insert( const Key& key, const Value& val )
    {
        std::shared_ptr<BucketsAccess> buckets_access; // Hold one reference of current buckets accessor.
        Bucket* bucket; // Get needed bucket.
        std::unique_lock<std::mutex> bucket_lock; // Lock on the bucket.
        get_bucket_with_lock( key, bucket, bucket_lock, buckets_access );

        std::unique_ptr<BucketItem>* first_bucket_item = &( bucket->first );

        if ( !insert_item_to_bucket( std::make_pair( key, val ), first_bucket_item ) )
        {
            return false;
        }

        ++m_size;

        // Check if it is a time for resizing.
        if ( size_t(std::sqrt(m_size.load())) > buckets_access->bucket_count )
        {
            // Unlock bucket_mutex and shared_ptr to buckets_access
            bucket_lock.unlock();
            buckets_access.reset();

            std::unique_lock<std::mutex> resizing_lock( m_resizing_lock, std::try_to_lock);
            if ( resizing_lock.owns_lock() )
            {
                resize( key );
            }
        }

        return true;
    }

    bool remove( const Key& key )
    {
        std::shared_ptr<BucketsAccess> buckets_access; // Hold one reference of current buckets accessor.
        Bucket* bucket; // Get needed bucket.
        std::unique_lock<std::mutex> bucket_lock; // Lock on the bucket.
        get_bucket_with_lock( key, bucket, bucket_lock, buckets_access );

        auto item_ptr = find_ptr_with_key( &( bucket->first ), key );

        if ( item_ptr->get() == nullptr )
        {
            return false;
        }

        *item_ptr = std::move( (*item_ptr)->next );

        --m_size;

        return true;
    }

    bool get( const Key& key, Value& out_val )
    {
        std::shared_ptr<BucketsAccess> buckets_access; // Hold one reference of current buckets accessor.
        Bucket* bucket; // Get needed bucket.
        std::unique_lock<std::mutex> bucket_lock; // Lock on the bucket.
        get_bucket_with_lock( key, bucket, bucket_lock, buckets_access );

        auto item_ptr = find_ptr_with_key( &( bucket->first ), key );

        if ( item_ptr->get() == nullptr )
        {
            return false;
        }

        out_val = (*item_ptr)->data.second;

        return true;
    }

    size_t size()
    {
        return m_size.load();
    }

private:

    void get_bucket_with_lock( const Key& key,
                               Bucket*& out_bucket,
                               std::unique_lock<std::mutex>& out_bucket_lock,
                               std::shared_ptr<BucketsAccess>& out_buckets_access )
    {
        do
        {
            if (out_bucket_lock.owns_lock())
            {
                out_bucket_lock.unlock();
            }
            out_buckets_access = m_buckets_access;
            out_bucket = get_bucket( key, out_buckets_access );
            out_bucket_lock = std::unique_lock<std::mutex>( out_bucket->lock );
        } while ( out_bucket->is_obsolete );
    }

    Bucket* get_bucket( const Key& key, std::shared_ptr<BucketsAccess>& buckets_access )
    {
        size_t bucket_ind = m_hash( key ) % buckets_access->bucket_count;
        return &(buckets_access->m_buckets[bucket_ind]);
    }

    // Must be protected by bucket mutex.
    bool insert_item_to_bucket( std::pair<Key, Value> data, std::unique_ptr<BucketItem>* first_bucket_item )
    {
        auto item_ptr = find_ptr_with_key( first_bucket_item, data.first );

        if ( item_ptr->get() != nullptr )
        {
            return false;
        }
        item_ptr->reset( new BucketItem{ data } );
        return true;
    }

    // Must be protected by bucket mutex.
    // Returns a raw pointer to the unique_ptr which refers to the needed BucketItem.
    std::unique_ptr<BucketItem>* find_ptr_with_key( std::unique_ptr<BucketItem>* item, const Key& key ) const
    {
        if ( (*item) == nullptr )
        {
            return item;
        }

        if ( m_comp( (*item)->data.first, key ) )
        {
            return item;
        }
        else
        {
            return find_ptr_with_key( &((*item)->next), key );
        }
    }

    // Must be called under the resizing mutex but not under the bucket mutex.
    void resize( const Key& key )
    {
        auto old_bucket_access = m_buckets_access; // Hold one reference of current buckets accessor.

        // Check the size again but now under the m_resizing_lock
        if ( size_t(std::sqrt(m_size.load())) <= old_bucket_access->bucket_count )
        {
            return;
        }

        // There is no research background related to resizing thresholds, scale factor and number of mutexes.
        size_t new_buckets_count = old_bucket_access->bucket_count * 2u;

        // Creating new bucket access.
        std::shared_ptr<BucketsAccess> new_bucket_access = std::shared_ptr<BucketsAccess>( new BucketsAccess );
        new_bucket_access->bucket_count = new_buckets_count;
        new_bucket_access->m_buckets = std::unique_ptr<Bucket[]>( new Bucket[new_buckets_count] );

        Bucket* new_buckets = new_bucket_access->m_buckets.get();

        // Lock all buckets in the new access.
        std::unique_lock<std::mutex> new_buckets_locks[new_buckets_count];

        for ( int i = 0; i != new_buckets_count; ++i )
        {
            new_buckets_locks[i] = std::unique_lock<std::mutex>( new_buckets[i].lock );
        }

        // Change m_buckets_access to the new bucket access.
        m_buckets_access = new_bucket_access;

        // Rehash items from old to new buckets.
        Bucket* old_buckets = old_bucket_access->m_buckets.get();
        for ( auto it = old_buckets; it != old_buckets + old_bucket_access->bucket_count; ++it )
        {
            // Lock old bucket.
            std::lock_guard<std::mutex> lock( it->lock );
            it->is_obsolete = true;

            BucketItem* bucket_item = it->first.get();
            while ( bucket_item )
            {
                auto& data = bucket_item->data;
                auto new_bucket = get_bucket( data.first, new_bucket_access );
                insert_item_to_bucket( data, &(new_bucket->first) );

                bucket_item = bucket_item->next.get();
            }
        }
    }

private:
    std::shared_ptr<BucketsAccess> m_buckets_access;
    const Hash m_hash;
    const Comp m_comp;
    std::atomic< size_t > m_size;
    std::mutex m_resizing_lock;
};

#endif // HASHMAP_H
