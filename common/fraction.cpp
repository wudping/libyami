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

Fraction::Fraction(int32_t num, int32_t den)
{
    m_den = den;
    m_num = num;
    if((0 != m_den) && (0 != m_num)){
        int32_t gcd = __gcd(m_num, m_den);
        m_num /= gcd;
        m_den /= gcd;
    }
}

void Fraction::operator += (const Fraction& data)
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

Fraction Fraction::operator + (const Fraction& data) const
{
    Fraction sum(m_num, m_den);
    sum += data;
    return sum;
}

void Fraction::operator -= (const Fraction& data)
{ 
    operator+=(data * (Fraction(-1, 1)));
}

Fraction Fraction::operator - (const Fraction& data) const
{ 
    Fraction sum(m_num, m_den);
    sum -= data;
    return sum;
}

void Fraction::operator *= (Fraction data)
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

Fraction Fraction::operator * (Fraction data) const
{
    Fraction result(m_num, m_den);
    result *= data;
    return result;
}

void Fraction::operator /= (Fraction data)
{
    if(!data.getNumerator()){
        cout << "Data illegal: divisor is zero. "<< endl;
        m_den = 0;
    }
    operator*=(Fraction(data.getDenominator(), data.getNumerator()));
}

Fraction Fraction::operator / (const Fraction& data) const
{
    if(!data.getNumerator()){
        cout << "Data illegal: divisor is zero. " << endl;
        return Fraction(0, 0);
    }
    Fraction result(m_num, m_den);
    result *= Fraction(data.getDenominator(), data.getNumerator());
    return result;
}

float Fraction::floatValue()  const
{
    if(getDenominator())
        return 1.0 * m_num / m_den;
    else
        return 0.0;
}

int32_t Fraction::intValue()  const
{
    if(getDenominator())
        return m_num / m_den;
    else
        return 0;
}






