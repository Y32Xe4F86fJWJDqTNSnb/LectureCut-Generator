#pragma once

#include <iterator>

template <typename T>
struct CircularIterator
{
    using difference_type = typename std::ptrdiff_t;
    using value_type = typename T;

    CircularIterator()
        : pointer(nullptr)
        , position(0)
        , length(1)
    {
    }

    CircularIterator & operator = (CircularIterator const & other) = default;
    CircularIterator(CircularIterator const & other) = default;
    CircularIterator(CircularIterator && other) = default;
    CircularIterator & operator = (CircularIterator && other) = default;
    
    CircularIterator(
        T * beginPointer,
        std::size_t startingPosition,
        std::size_t length
    )
        : pointer(beginPointer + startingPosition)
        , position(startingPosition)
        , length(length)
    {
    }

    T & operator * () const
    {
        return *pointer;
    }

    CircularIterator & operator ++ ()
    {
        ++position;
        if(position == length) 
        {
            position = 0;
            pointer -= length - 1;
            return *this;
        }
        else 
        {
            ++pointer;
            return *this;
        }
    }

    CircularIterator operator ++ (int)
    {
        CircularIterator oldIterator(*this);
        this->operator ++ ();
        return oldIterator;
    }

    CircularIterator & operator += (std::size_t increment)
    {
        if(increment > length)
            increment = increment % length;
        position += increment;
        if(position >= length) 
        {
            position -= length;
            pointer -= length - increment;
            return *this;
        }
        else 
        {
            pointer += increment;
            return *this;
        }
    }

    CircularIterator operator + (std::size_t increment)
    {
        CircularIterator output(*this);
        output += increment;
        return output;
    }

    CircularIterator & operator -- ()
    {
        if(position == 0) 
        {
            position = length - 1;
            pointer += position;
            return *this;
        }
        else 
        {
            --position;
            --pointer;
            return *this;
        }
    }

    CircularIterator operator -- (int)
    {
        CircularIterator oldIterator(*this);
        this->operator -- ();
        return oldIterator;
    }

    CircularIterator & operator -= (std::size_t decrement)
    {
        if(position < decrement) 
        {
            if(decrement > length)
                decrement = decrement % length;
            position += length - decrement;
            pointer += length - decrement;
            return *this;
        }
        else 
        {
            position -= decrement;
            pointer -= decrement;
            return *this;
        }
    }

    CircularIterator operator - (std::size_t decrement)
    {
        CircularIterator output(*this);
        output -= decrement;
        return output;
    }

    bool operator == (CircularIterator const & other) const 
    {
        return
            pointer == other.pointer
            && position == other.position
            && length == other.length;
    }

protected:
    T * pointer;
    std::size_t position, length;
};

static_assert(std::incrementable<CircularIterator<std::size_t>>);
static_assert(std::input_iterator<CircularIterator<std::size_t>>);
static_assert(std::output_iterator<CircularIterator<std::size_t>, double>);
static_assert(std::forward_iterator<CircularIterator<std::size_t>>);
static_assert(std::bidirectional_iterator<CircularIterator<std::size_t>>);