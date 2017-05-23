/*
 * Copyright (C) 2014-2016 Intel Corporation. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "fraction.h"

using namespace std;

CFraction::CFraction(int32_t num, int32_t den)
{
    m_den = den;
    m_num = num;
    if((0 != m_den) && (0 != m_num)){
        int32_t gcd = __gcd(m_num, m_den);
        m_num /= gcd;
        m_den /= gcd;
    }
}

void CFraction::operator += (const CFraction& data)
{
    if(!m_den || !data.getDenominator()){
        cout << "Data illegal: " << m_den << ", or " << data.getDenominator() << endl;
        m_den = 0;
        return ;
    }
    int32_t den = m_den * data.getDenominator();
    int32_t num = m_num * data.getDenominator() + m_den * data.getNumerator();
    if(0 == num){
        m_num = 0;
        m_den = 1;
        return ;
    }
    int32_t gcd = __gcd(num, den);
    m_num = num/gcd;
    m_den = den/gcd;
}

CFraction CFraction::operator + (const CFraction& data) const
{
    CFraction sum(m_num, m_den);
    sum += data;
    return sum;
}

void CFraction::operator -= (const CFraction& data)
{ 
    operator+=(data * (CFraction(-1, 1)));
}

CFraction CFraction::operator - (const CFraction& data) const
{ 
    CFraction sum(m_num, m_den);
    sum -= data;
    return sum;
}

void CFraction::operator *= (const CFraction& data)
{
    if(!m_den || !data.getDenominator()){
        cout << "Data illegal: " << m_den << ", or " << data.getDenominator() << endl;
        m_den = 0;
        return ;
    }
    int32_t num = m_num * data.getNumerator();
    int32_t den = m_den * data.getDenominator();
    if(0 == num){
        m_num = 0;
        m_den = 1;
        return ;
    }
    int32_t gcd = __gcd(num, den);
    m_num = num/gcd;
    m_den = den/gcd;
}

CFraction CFraction::operator * (const CFraction& data) const
{
    CFraction result(m_num, m_den);
    result *= data;
    return result;
}

void CFraction::operator /= (const CFraction& data)
{
    if(!data.getNumerator()){
        cout << "Data illegal: divisor is zero. "<< endl;
        m_den = 0;
    }
    operator*=(CFraction(data.getDenominator(), data.getNumerator()));
}

CFraction CFraction::operator / (const CFraction& data) const
{
    if(!data.getNumerator()){
        cout << "Data illegal: divisor is zero. " << endl;
        return CFraction(0, 0);
    }
    CFraction result(m_num, m_den);
    result *= CFraction(data.getDenominator(), data.getNumerator());
    return result;
}

float CFraction::floatValue()  const
{
    if(getDenominator())
        return 1.0 * m_num / m_den;
    else
        return 0.0;
}

int32_t CFraction::intValue()  const
{
    if(getDenominator())
        return m_num / m_den;
    else
        return 0;
}






